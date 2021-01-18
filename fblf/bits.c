
#include <stdlib.h>   // for malloc, free
#include <string.h>   // for strlen

#include "bits.h"

typedef enum {
  BIT_0, BIT_1
} Bit;

struct FblfBits {
  size_t unused;
  Bit bits[];
};

// FblfNewBits -- see documentation in bits.h
FblfBits* FblfNewBits(size_t n)
{
  return (FblfBits*)malloc(sizeof(FblfBits) + n * sizeof(Bit));
}

// FblfNewBitsFromBinary -- see documentation in bits.h
FblfBits* FblfNewBitsFromBinary(const char* bitstr)
{
  size_t n = strlen(bitstr);
  FblfBits* bits = FblfNewBits(n);
  for (size_t i = 0; i < n; ++i) {
    bits->bits[i] = bitstr[i] - '0';
  }
  return bits;
}

// FblfFreeBits -- see documentation in bits.h
void FblfFreeBits(FblfBits* bits)
{
  free(bits);
}

// FblfGetBitRef -- see documentation in bits.h
FblfBitRef FblfGetBitRef(FblfBits* bits, size_t i)
{
  FblfBitRef ref = { .bits = bits, .i = i };
  return ref;
}

// FblfCopyBits -- See documentation in bits.h
void FblfCopyBits(FblfBitRef dest, FblfBitRef src, size_t count)
{
  // TODO: Properly handle aliasing/overlap case.
  for (size_t i = 0; i < count; ++i) {
    dest.bits->bits[dest.i + i] = src.bits->bits[src.i + i];
  }
}

// FblfBitsEqual -- See documentation in bits.h
bool FblfBitsEqual(FblfBitRef a, FblfBitRef b, size_t count)
{
  for (size_t i = 0; i < count; ++i) {
    if (a.bits->bits[a.i + i] != b.bits->bits[b.i + i]) {
      return false;
    }
  }
  return true;
}
