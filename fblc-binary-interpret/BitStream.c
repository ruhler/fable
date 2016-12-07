// BitStream.c --
//
//   This file implements routines for reading and writing bit streams.


struct BitStream {
  FILE* byte_stream;
  uint64_t pending_bits;
  int num_pending_bits;
};

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
//   The behavior is undefined if num_bits is less than 0 or greater than 31.

uint32_t ReadBits(BitStream* stream, int num_bits) {
  assert(num_bits >= 0 && num_bits < 32 && "FblciReadBits invalid argument");

  // Read in more bits until we can satisfy the request.
  while (stream->num_pending_bits < num_bits) {
    int c = fgetc(stream->byte_stream);
    if (c == EOF) {
      return EOF;
    }

    assert(c >= 0 && c <= 0xFF && "Unexpected result of fgetc");
    stream->num_pending_bits += 8;
    stream->pending_bits = (stream->pending_bits << 8) | c;
  }

  stream->num_pending_bits -= num_bits;
  uint32_t bits = stream->pending_bits >> stream->num_pending_bits;
  stream->pending_bits &= (1 << stream->num_pending_bits) - 1;
  return bits;
}
