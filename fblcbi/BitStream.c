// BitStream.c --
//
//   This file implements routines for reading and writing bit streams.

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "fblc.h"

struct BitSource {
  const char* string;
  int fd;
  bool synced;
};

BitSource* CreateStringBitSource(FblcArena* arena, const char* string)
{
  BitSource* source = arena->alloc(arena, sizeof(BitSource));
  source->string = string;
  source->fd = -1;
  source->synced = false;
  return source;
}

BitSource* CreateFdBitSource(FblcArena* arena, int fd)
{
  BitSource* source = arena->alloc(arena, sizeof(BitSource));
  source->string = NULL;
  source->fd = fd;
  source->synced = false;
  return source;
}

// ReadBit --
//   Read the next bit from the given bit stream.
//
// Inputs:
//   stream - the bit stream to read from.
//
// Returns:
//   The next bit in the stream. EOF if the end of the stream has been
//   reached.
//
// Side effects:
//   Advance the stream by a single bit.

static int ReadBit(BitSource* source)
{
  source->synced = true;
  int bit = EOF;
  if (source->string != NULL && *source->string != '\0') {
    bit = *source->string;
    source->string++;
  } else if (source->fd >= 0) {
    char c;
    if(read(source->fd, &c, 1)) {
      bit = c;
    }
  }

  switch (bit) {
    case '0': return 0;
    case '1': return 1;
    case EOF: return EOF;

    default:
      fprintf(stderr, "Unexpected char in bit source: '%c'", bit);
      assert(false);
      return EOF;
  }
}

// ReadBits --
//
//   Read num_bits bits from the given bit stream.
//
// Inputs:
//   stream - the bit stream to read from.
//   num_bits - the number of bits to read.
//
// Returns:
//   The next num_bits bits in the stream, zero extended. Returns EOF on error
//   or if there are insufficient bits remaining in the file to satisfy the
//   request.
//
// Side effects:
//   Advance the stream by num_bits bits.
//   The behavior is undefined if num_bits is greater than 31.

uint32_t ReadBits(BitSource* source, size_t num_bits)
{
  assert(num_bits < 32 && "ReadBits invalid argument");

  uint32_t bits = 0;
  for (size_t i = 0; i < num_bits; ++i) {
    int bit = ReadBit(source);
    if (bit == EOF) {
      return EOF;
    }
    bits = (bits << 1) | bit;
  }
  return bits;
}
void SyncBitSource(BitSource* source)
{
  if (!source->synced) {
    ReadBit(source);
  }
}
void FreeBitSource(FblcArena* arena, BitSource* source)
{
  arena->free(arena, source);
}

// OpenBinaryOutputBitStream
//   Open an OutputBitStream that writes ascii digits '0' and '1' to an open
//   file.
//
// Inputs:
//   stream - The stream to open.
//   fd - The file descriptor of an open file to write to.
//
// Returns:
//   None.
//
// Side effects:
//   The bit stream set to write to the given file.

void OpenBinaryOutputBitStream(OutputBitStream* stream, int fd)
{
  stream->fd = fd;
  stream->flushed = false;
}

// WriteBits --
//
//   Write num_bits bits to the given bit stream.
//
// Inputs:
//   stream - the bit stream to write to.
//   num_bits - the number of bits to write. This must be less than 32.
//   bits - the actual bits to write.
//
// Returns:
//   None.
//
// Side effects:
//   num_bits bits are written to the stream in the form of binary ascii
//   digits '0' and '1'.
void WriteBits(OutputBitStream* stream, size_t num_bits, uint32_t bits)
{
  assert(num_bits < 32 && "WriteBits invalid num_bits");
  for (uint32_t mask = ((1 << num_bits) >> 1); mask > 0; mask >>= 1) {
    char c = (bits & mask) ? '1' : '0';
    write(stream->fd, &c, 1);
    stream->flushed = true;
  }
}

// FlushWriteBits --
//
//   Flush bits as necessary to mark the end of a value.
//
// Inputs:
//   stream - the bit stream to flush the bits in.
//
// Returns:
//   None.
//
// Side effects:
//   If necessary, write padding bits to the stream to indicate the end of a
//   value has been reached.
void FlushWriteBits(OutputBitStream* stream)
{
  if (!stream->flushed) {
    WriteBits(stream, 1, 0);
  }
}
