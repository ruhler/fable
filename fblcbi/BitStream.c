// BitStream.c --
//
//   This file implements routines for reading and writing bit streams.


struct BitStream {
  FILE* byte_stream;
  uint64_t pending_bits;
  size_t num_pending_bits;
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
//   The behavior is undefined if num_bits is greater than 31.

uint32_t ReadBits(BitStream* stream, size_t num_bits)
{
  assert(num_bits < 32 && "ReadBits invalid argument");

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

void WriteBits(BitStream* stream, size_t num_bits, uint32_t bits)
{
  assert(num_bits < 32 && "WriteBits invalid num_bits");
  assert((bits >> num_bits) == 0 && "WriteBits invalid bits");

  stream->pending_bits = (stream->pending_bits << num_bits) | bits;
  stream->num_pending_bits += num_bits;

  // Write out bits as long as we have enough to write out.
  while (stream->num_pending_bits >= 8) {
    stream->num_pending_bits -= 8;
    int c = stream->pending_bits >> stream->num_pending_bits;
    fputc(c, stream->byte_stream);
    stream->pending_bits &= (1 << stream->pending_bits) - 1;
  }
}
