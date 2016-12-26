// BitStream.c --
//
//   This file implements routines for writing bit streams.

#include <assert.h>     // for assert.
#include <unistd.h>     // for write.

#include "Internal.h"

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

  for (uint32_t mask = (1 << (num_bits - 1)); mask > 0; mask >>= 1) {
    char c = (bits & mask) ? '1' : '0';
    write(stream->fd, &c, 1);
  }
}
