/**
 * @file pprof.c
 *  Writes an fble profile in google/pprof format.
 */

#include <string.h>   // for strlen

#include <fble/fble-profile.h>

// The pprof proto format is specified at proto/profile.proto in the
// github.com/google/pprof project. Maybe you can find it at:
// https://github.com/google/pprof/blob/main/proto/profile.proto
//
// The proto3 protobuf encoding format is specified at:
// https://protobuf.dev/programming-guides/encoding/
//
// We use a straight forward mapping from FbleBlockId to other ids:
//   location_id = FbleBlockId + 1
//   function_id = FbleBlockId + 1
//   string_id for samples sample type = 1
//   string_id for samples sample unit = 2
//   string_id for calls sample type = 3
//   string_id for calls sample unit = 4
//   string_id for block name = 2 * FbleBlockId + 5
//   string_id for file name = 2 * FbleBlockId + 6


static uint64_t VarIntLength(uint64_t x);
static uint64_t TagLength(uint64_t x);
static uint64_t TaggedVarIntLength(uint64_t field, uint64_t value);
static uint64_t TaggedLenLength(uint64_t field, uint64_t length);

static void VarInt(FILE* fout, uint64_t value);
static void TaggedVarInt(FILE* fout, uint64_t field, uint64_t value);
static void TaggedLen(FILE* fout, uint64_t field, uint64_t len);

static void SampleType(FILE* fout, uint64_t type, uint64_t unit);
static void Location(FILE* fout, uint64_t location_id, uint64_t func_id, uint64_t line, uint64_t col);
static void Function(FILE* fout, uint64_t func_id, uint64_t name_string_id, uint64_t file_string_id, uint64_t line);
static void StringTable(FILE* fout, const char* str);

static void SampleQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t calls, uint64_t samples);

/**
 * @func[VarIntLength] Returns the length of a varint.
 *  @arg[uint64_t][x] The integer value to get the length of.
 *  @returns[uint64_t] The number of bytes needed to represent it.
 */
static uint64_t VarIntLength(uint64_t x) {
  uint64_t len = 1;
  while (x >= 0x80) {
    len++;
    x >>= 7;
  }
  return len;
}

/**
 * @func[TagLength] Returns the length of a tag.
 *  @arg[uint64_t][x] The id of the field in the tag.
 *  @returns[uint64_t] The number of bytes needed to the tag.
 */
static uint64_t TagLength(uint64_t x) {
  return VarIntLength(x << 3);
}

/**
 * @func[TaggedVarIntLength] Returns the length of a tagged varint.
 *  @arg[uint64_t][field] The id of the field in the tag.
 *  @arg[uint64_t][value] The value of the varint.
 *  @returns[uint64_t] The number of bytes needed for the record.
 */
static uint64_t TaggedVarIntLength(uint64_t field, uint64_t value) {
  return TagLength(field) + VarIntLength(value);
}

/**
 * @func[TaggedLenLength] Returns the length of a tagged len.
 *  @arg[uint64_t][field] The id of the field in the tag.
 *  @arg[uint64_t][value] The value of the length.
 *  @returns[uint64_t] The number of bytes needed for the record.
 */
static uint64_t TaggedLenLength(uint64_t field, uint64_t value) {
  return TagLength(field) + VarIntLength(value);
}

/**
 * @func[VarInt] Output a varint to the file.
 *  @arg[FILE*][fout] The file to output it to.
 *  @arg[uint64_t][value] The value to output.
 */
static void VarInt(FILE* fout, uint64_t value)
{
  do {
    uint8_t byte = value & 0x7F;
    value >>= 7;

    if (value > 0) {
      byte |= 0x80;
    }

    fwrite(&byte, sizeof(uint8_t), 1, fout);
  } while (value > 0);
}

/**
 * @func[TaggedVarInt] Outputs a tagged varint to the file.
 *  @arg[FILE*][fout] The file to output to
 *  @arg[uint64_t][field] The field value to put in the tag.
 *  @arg[uint64_t][value] The value to output.
 *  @sideeffects Writes a tagged varint record to fout.
 */
static void TaggedVarInt(FILE* fout, uint64_t field, uint64_t value)
{
  VarInt(fout, (field << 3) | 0);   // VARINT = 0
  VarInt(fout, value);
}

/**
 * @func[TaggedLen] Outputs a tagged length to the file.
 *  @arg[FILE*][fout] The file to output to
 *  @arg[uint64_t][field] The field value to put in the tag.
 *  @arg[uint64_t][length] The length value to output.
 *  @sideeffects Writes a tagged length record to fout.
 */
static void TaggedLen(FILE* fout, uint64_t field, uint64_t len)
{
  VarInt(fout, (field << 3) | 2);   // LEN = 2
  VarInt(fout, len);
}

/**
 * @func[SampleType] Emits a ValueType sample_type field record.
 *  @arg[FILE*][fout] The output file.
 *  @arg[uint64_t][type] String id of the type field.
 *  @arg[uint64_t][unit] String id of the unit field.
 *  @sideeffects Writes the record to fout.
 */
static void SampleType(FILE* fout, uint64_t type, uint64_t unit)
{
  uint64_t len = 0;
  len += TaggedVarIntLength(1, type); // .type = 1
  len += TaggedVarIntLength(2, unit); // .unit = 2

  TaggedLen(fout, 1, len);      // .sample_type = 1
  TaggedVarInt(fout, 1, type);  // .type = 1
  TaggedVarInt(fout, 2, unit);  // .unit = 2
}

/**
 * @func[Location] Output a location record
 *  @arg[FILE*][fout] The file to output to
 *  @arg[uint64_t][location_id] The id of the location.
 *  @arg[uint64_t][func_id] The id of the function associated with the location.
 *  @arg[uint64_t][line] The line number of the location.
 *  @arg[uint64_t][col] The column number of the location.
 *  @sideeffects Writes a Location record to fout.
 */
static void Location(FILE* fout, uint64_t location_id, uint64_t func_id, uint64_t line, uint64_t col)
{
  uint64_t line_len = 0;
  line_len += TaggedVarIntLength(1, func_id); // .function_id = 1
  line_len += TaggedVarIntLength(2, line);    // .line = 2
  line_len += TaggedVarIntLength(3, col);     // .column = 3

  uint64_t len = 0;
  len += TaggedVarIntLength(1, location_id);  // .id = 1
  len += TaggedLenLength(4, line_len);        // .line = 4
  len += line_len;

  TaggedLen(fout, 4, len);              // .location = 4
  TaggedVarInt(fout, 1, location_id);   // .id = 1

  TaggedLen(fout, 4, line_len);         // .line = 4
  TaggedVarInt(fout, 1, func_id);       // .function_id = 1
  TaggedVarInt(fout, 2, line);          // .line = 2
  TaggedVarInt(fout, 3, col);           // .column = 3
}

/**
 * @func[Function] Output a function record.
 *  @arg[FILE*][fout] The file to output to.
 *  @arg[uint64_t][func_id] The id of the function.
 *  @arg[uint64_t][name_string_id] string id of the function name.
 *  @arg[uint64_t][file_string_id] string id of the file the function is in.
 *  @arg[uint64_t][line] line number for the function.
 *  @sideeffects Writes a Function record to fout.
 */
static void Function(FILE* fout, uint64_t func_id, uint64_t name_string_id, uint64_t file_string_id, uint64_t line)
{
  uint64_t len = 0;
  len += TaggedVarIntLength(1, func_id);          // .id = 1
  len += TaggedVarIntLength(2, name_string_id);   // .name = 2
  len += TaggedVarIntLength(3, name_string_id);   // .system_name = 3
  len += TaggedVarIntLength(4, file_string_id);   // .filename = 4
  len += TaggedVarIntLength(5, line);             // .start_line = 5

  TaggedLen(fout, 5, len);                // .function = 5
  TaggedVarInt(fout, 1, func_id);         // .id = 1
  TaggedVarInt(fout, 2, name_string_id);  // .name = 2
  TaggedVarInt(fout, 3, name_string_id);  // .system_name = 3
  TaggedVarInt(fout, 4, file_string_id);  // .filename = 4
  TaggedVarInt(fout, 5, line);            // .start_line = 5
}

/**
 * @func[StringTable] Outputs a string table entry.
 *  @arg[FILE*][fout] The file to output to.
 *  @arg[const char*][str] The string to output.
 *  @sideeffects Writes a StringTable entry to fout.
 */
static void StringTable(FILE* fout, const char* str)
{
  uint64_t len = strlen(str);

  VarInt(fout, (6 << 3) | 2);   // .string_table = 6, LEN = 2
  VarInt(fout, len);
  fwrite(str, sizeof(char), len, fout);
}

/**
 * @func[SampleQuery] Query to use for outputting samples.
 *  See documentation of FbleProfileQuery in fble-profile.h
 */
static void SampleQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t calls, uint64_t samples)
{
  FILE* fout = (FILE*)userdata;

  uint64_t len = 0;
  for (size_t i = 0; i < seq.size; ++i) {
    FbleBlockId id = seq.xs[seq.size - i -1];
    len += TaggedVarIntLength(1, id + 1); // .location_id = 1
  }
  len += TaggedVarIntLength(2, calls);    // .value = 2
  len += TaggedVarIntLength(2, samples);  // .value = 2

  TaggedLen(fout, 2, len);          // .sample = 2
  for (size_t i = 0; i < seq.size; ++i) {
    FbleBlockId id = seq.xs[seq.size - i -1];
    TaggedVarInt(fout, 1, id + 1);  // .location_id = 1;
  }
  TaggedVarInt(fout, 2, calls);     // .value = 2
  TaggedVarInt(fout, 2, samples);   // .value = 2
}

// See documentation in fble-profile.h.
void FbleOutputProfile(FILE* fout, FbleProfile* profile)
{
  if (!profile->enabled) {
    return;
  }

  // sample_type fields
  SampleType(fout, 1, 2); // ["calls", "count"]
  SampleType(fout, 3, 4); // ["samples", "count"]

  // sample fields.
  FbleQueryProfile(profile, &SampleQuery, (void*)fout);

  // location fields.
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    FbleName name = profile->blocks.xs[i];
    Location(fout, i + 1, i + 1, name.loc.line, name.loc.col);
  }

  // function fields.
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    FbleName name = profile->blocks.xs[i];
    Function(fout, i + 1, 2 * i + 5, 2 * i + 6, name.loc.line);
  }

  // string_table fields.
  StringTable(fout, "");
  StringTable(fout, "calls");
  StringTable(fout, "count");
  StringTable(fout, "samples");
  StringTable(fout, "count");
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    FbleName name = profile->blocks.xs[i];
    StringTable(fout, name.name->str);
    StringTable(fout, name.loc.source->str);
  }

  fflush(fout);
}
