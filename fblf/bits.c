
#include <assert.h>   // for assert
#include <string.h>   // for strlen

#include "bits.h"

#define BITS_PER_WORD 64
typedef uint64_t Word;

#define HEAP_SIZE_IN_BITS (1024 * 1024 * 1024)
#define HEAP_SIZE_IN_WORDS (HEAP_SIZE_IN_BITS / BITS_PER_WORD)
static Word gHeap[HEAP_SIZE_IN_WORDS];

// TODO: Currently we use a bump pointer scheme where we just leak
// allocations. Start tracking allocations that have been freed so we can
// reclaim their memory for subsequent allocations.
static FblfBitPtr gNextFree;

// FblfNewBits -- see documentation in bits.h
FblfBitPtr FblfNewBits(size_t n)
{
  assert(gNextFree + n < HEAP_SIZE_IN_BITS && "OUT OF BITS MEMORY");
  FblfBitPtr ptr = gNextFree;
  gNextFree += n;
  return ptr;
}

// FblfNewBitsFromBinary -- see documentation in bits.h
FblfBitPtr FblfNewBitsFromBinary(const char* binstr)
{
  size_t n = strlen(binstr);
  FblfBitPtr bits = FblfNewBits(n);
  for (size_t i = 0; i < n; ++i) {
    uint64_t bit = (binstr[i] == '0') ? 0 : 1;
    FblfSetBits(bits+i, 1, bit);
  }
  return bits;
}

FblfBitPtr FblfNewBitsFromHex(const char* hexstr)
{
  size_t n = strlen(hexstr);
  FblfBitPtr bits = FblfNewBits(4 * n);
  for (size_t i = 0; i < n; ++i) {
    uint64_t value;
    if (hexstr[i] >= '0' && hexstr[i] <= '9') {
      value = hexstr[i] - '0';
    } else if (hexstr[i] >= 'A' && hexstr[i] <= 'F') {
      value = 10 + hexstr[i] - 'A';
    } else if (hexstr[i] >= 'a' && hexstr[i] <= 'f') {
      value = 10 + hexstr[i] - 'a';
    } else {
      assert(false && "non-hex digit");
    }
    FblfSetBits(bits+4*i, 4, value);
  }
  return bits;
}

// FblfFreeBits -- see documentation in bits.h
void FblfFreeBits(FblfBitPtr ptr)
{
  // TODO: Do something useful here!
}

// FblfGetBits -- see documentation in bits.h
uint64_t FblfGetBits(FblfBitPtr ptr, size_t n)
{
  assert(n <= 64 && "Invalid argument");
  size_t q = ptr / BITS_PER_WORD;
  size_t r = ptr % BITS_PER_WORD;
  Word w0 = gHeap[q];
  Word w1 = gHeap[q+1];

  Word w0_bits = w0 << r;
  Word w1_bits = w1 >> (BITS_PER_WORD - r);
  return (w0_bits | w1_bits) >> (BITS_PER_WORD - n);
}

// FblfSetBits -- see documentation in bits.h
void FblfSetBits(FblfBitPtr ptr, size_t n, uint64_t value)
{
  assert(n <= 64 && "Invalid argument");
  size_t unused_value_bits = BITS_PER_WORD - n;
  size_t q = ptr / BITS_PER_WORD;
  size_t r = ptr % BITS_PER_WORD;
  Word mask = ~((Word)0);

  // Extract, align, and overwrite the bits of value that go in word 0.
  Word w0 = gHeap[q];
  Word v0 = value << unused_value_bits;
  Word m0 = mask << unused_value_bits;
  v0 = v0 >> r;
  m0 = m0 >> r;
  gHeap[q] = (w0 & (~m0)) | v0;

  // Extract, align, and overwrite the bits of value that go in word 1.
  size_t non_w1_bits = unused_value_bits + (BITS_PER_WORD - r);
  Word w1 = gHeap[q+1];
  Word v1 = value << non_w1_bits;
  Word m1 = mask << non_w1_bits;
  gHeap[q+1] = (w1 & (~m1)) | v1;
}

// FblfCopyBits -- See documentation in bits.h
void FblfCopyBits(FblfBitPtr src, FblfBitPtr dst, size_t n)
{
  while (n > BITS_PER_WORD) {
    FblfSetBits(dst, BITS_PER_WORD, FblfGetBits(src, BITS_PER_WORD));
    dst += BITS_PER_WORD;
    src += BITS_PER_WORD;
    n -= BITS_PER_WORD;
  }
  FblfSetBits(dst, n, FblfGetBits(src, n));
}

// FblfBitsEqual -- See documentation in bits.h
bool FblfBitsEqual(FblfBitPtr a, FblfBitPtr b, size_t n)
{
  while (n > BITS_PER_WORD) {
    if (FblfGetBits(a, BITS_PER_WORD) != FblfGetBits(b, BITS_PER_WORD)) {
      return false;
    }
    a += BITS_PER_WORD;
    b += BITS_PER_WORD;
    n -= BITS_PER_WORD;
  }
  return FblfGetBits(a, n) == FblfGetBits(b, n);
}
