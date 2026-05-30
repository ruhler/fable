/**
 * @file literal.c
 *  Routines for encoding and decoding literal values.
 */

#include "literal.h"

#include <assert.h>   // for assert
#include <string.h>   // for strlen, strncmp

#include <fble/fble-vector.h>   // for FbleInitVector, FbleFreeVector

#include "tc.h"       // for FbleTagWidth

// The encoding for literal values is a mini program.
//
// The program constructs a literal starting with a Unit value and empty
// list, then interpreting bytes from the program in order as follows:
//
// First byte is [WWWWWWW, L]:
//  * The most least significant bit L is set to 1 to indicate this is the
//    last tag value before adding the element to the list and resetting the
//    argument to Unit. The bit L is set to 0 to indicate the value needs to
//    be wrapped by more tag values before adding it to the list.
//  * The most significant 7 bits WWWWWWW specifies the tag width of the union
//    value to wrap the current argument value in.
//
// Followed by as few bytes as needed to encode the tag value given the tag
// width, most significant byte to least significant byte.

typedef struct {
  size_t size;
  uint8_t* xs;
} ByteV;

/**
 * @struct[TypeList] Linked list of types to detect recursion.
 *  @field[FbleType*][type] The current element.
 *  @field[TypeList*][next] The rest of the elements.
 */
typedef struct TypeList {
  FbleType* type;
  struct TypeList* next;
} TypeList;

static void AppendTag(size_t tagwidth, size_t tag, bool last, ByteV* data);
static const char* GetNextLetter(FbleTypeHeap* th, FbleType* type, const char* word, ByteV* data, TypeList* tl);
static const char* ParseLiteral(FbleTypeHeap* th, FbleType* type, const char* word, ByteV* data);

/**
 * @func[AppendTag] Appends an encoded tag header and value to data stream.
 *  @arg[size_t][tagwidth] The width of the tag.
 *  @arg[size_t][tag] The tag value.
 *  @arg[bool][last] True if this is the last tag to add for an element.
 *  @arg[ByteV*][data] The data to append to.
 *  @sideeffects Appends the tag value encoded to the data stream.
 */
static void AppendTag(size_t tagwidth, size_t tag, bool last, ByteV* data)
{
  uint8_t header = tagwidth << 1;
  if (last) {
    header |= 0x1;
  }
  FbleAppendToVector(*data, header);

  // Encoding is minimum number of bytes for the given tag width, most
  // significant byte of the tag first.
  if (tagwidth > 24) {
    FbleAppendToVector(*data, (tag >> 24) & 0xFF);
  }
  if (tagwidth > 16) {
    FbleAppendToVector(*data, (tag >> 16) & 0xFF);
  }
  if (tagwidth > 8) {
    FbleAppendToVector(*data, (tag >> 8) & 0xFF);
  }
  if (tagwidth > 0) {
    FbleAppendToVector(*data, (tag >> 0) & 0xFF);
  }
}

/**
 * @func[GetNextLetter] Finds the next letter value for a literal.
 *  @arg[FbleTypeHeap*][th] The type heap
 *  @arg[FbleType*][type] The letter type.
 *  @arg[const char*][word] The literal word to get the next letter in.
 *  @arg[TagV*][data]
 *   Output where the matched letter will be placed in literal data format.
 *  @arg[TypeList*][tl] List of type args on stack to detect recursion
 *  @returns[const char*]
 *   The rest of the word after the next letter. NULL if no match was found.
 *  @sideeffects Writes literal data for the letter to data if a match was found.
 */
static const char* GetNextLetter(FbleTypeHeap* th, FbleType* type, const char* word, ByteV* data, TypeList* tl)
{
  FbleDataType* dt = (FbleDataType*)FbleNormalType(th, type);
  if (dt->_base.tag != FBLE_DATA_TYPE || dt->datatype != FBLE_UNION_DATATYPE) {
    // Letters can only be found in union types.
    FbleReleaseType(th, &dt->_base);
    return NULL;
  }

  for (TypeList* l = tl; l != NULL; l = l->next) {
    if (&dt->_base == l->type) {
      // We've circled back recursively to a type we've already started
      // searching in. There's nothing new we'll find this time around.
      FbleReleaseType(th, &dt->_base);
      return NULL;
    }
  }

  // If we have a null type list, it means we are out the outer most layer of
  // union types, so this will be the last tag we need before adding the
  // argument to the list of literal elements.
  bool last = (tl == NULL);
  size_t tagwidth = FbleTagWidth(dt->fields.size);

  for (size_t i = 0; i < dt->fields.size; ++i) {
    // Match letter against a Unit field.
    if (FbleIsUnitType(th, dt->fields.xs[i].type)) {
      const char* fieldname = dt->fields.xs[i].name.name->str;
      size_t len = strlen(fieldname);
      if (len > 0 && strncmp(word, fieldname, len) == 0) {
        AppendTag(tagwidth, i, last, data);
        FbleReleaseType(th, &dt->_base);
        return word + len;
      }
    }

    // Search for letter in a non-Unit subfield.
    TypeList ntl = { .type = &dt->_base, .next = tl };
    const char* next = GetNextLetter(th, dt->fields.xs[i].type, word, data, &ntl);
    if (next != NULL) {
      AppendTag(tagwidth, i, last, data);
      FbleReleaseType(th, &dt->_base);
      return next;
    }
  }

  FbleReleaseType(th, &dt->_base);
  return NULL;
}

/**
 * @func[ParseLiteral] Parse the rest of a literal.
 *  @arg[FbleTypeHeap*][th] The type heap
 *  @arg[FbleType*][type] The letter type.
 *  @arg[const char*][word] The literal word to parse.
 *  @arg[TagV*][data] Output for parsed literal data.
 *  @returns[const char*]
 *   NULL on success. Otherwise points to where in the literal an error was
 *   found.
 *  @sideeffects Appends literal data to data.
 */
static const char* ParseLiteral(FbleTypeHeap* th, FbleType* type, const char* word, ByteV* data)
{
  if (*word == '\0') {
    return NULL;  // Nothing to parse. Return success.
  }

  ByteV letter;
  FbleInitVector(letter);

  const char* next = GetNextLetter(th, type, word, &letter, NULL);
  if (next == NULL) {
    FbleFreeVector(letter);
    return word;  // error at this location
  }

  const char* err = ParseLiteral(th, type, next, data);
  if (err != NULL) {
    FbleFreeVector(letter);
    return err;   // propagate the error
  }

  for (size_t i = 0; i < letter.size; ++i) {
    FbleAppendToVector(*data, letter.xs[i]);
  }
  FbleFreeVector(letter);
  return NULL;    // success
}

FbleLiteral FbleParseLiteral(FbleTypeHeap* th, FbleType* type, const char* word)
{
  ByteV data;
  FbleInitVector(data);

  const char* err = ParseLiteral(th, type, word, &data);
  if (err != NULL) {
    FbleFreeVector(data);
    FbleLiteral failed = { .size = err - word, .data = NULL };
    return failed;
  }

  FbleLiteral literal = { .size = data.size, .data = data.xs };
  // fprintf(stderr, "%zi\n", literal.size);
  return literal;
}

// See documentation in fble-literal.h.
FbleValue* FbleNewLiteralValue(FbleValueHeap* heap, size_t size, uint8_t* data)
{
  uint8_t* end = data + size;

  FbleValue* unit = FbleNewStructValue_(heap, 0);
  FbleValue* list = FbleNewUnionValue(heap, 1, 1, unit);
  FbleValue* arg = unit;
  while (data < end) {
    uint8_t header = *data++;
    size_t tagwidth = header >> 1;
    bool last = header & 0x1;

    size_t tag = 0;
    for (size_t i = 0; 8 * i < tagwidth; i++) {
      assert(data < end && "malformed literal data");
      tag <<= 8;
      tag |= *data++;
    }

    arg = FbleNewUnionValue(heap, tagwidth, tag, arg);
    if (last) {
      FbleValue* cons = FbleNewStructValue_(heap, 2, arg, list);
      list = FbleNewUnionValue(heap, 1, 0, cons);
      arg = unit;
    }
  }
  return list;
}
