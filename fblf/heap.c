
#include "heap.h"

// See documentation of FblfHeapRead in heap.h.
FblfHeapWord FblfHeapRead(FblfHeap* heap, FblfHeapAddr addr, size_t n)
{
  assert(n <= FBLF_HEAP_BITS_PER_WORD && "Invalid argument");
  size_t q = addr / FBLF_HEAP_BITS_PER_WORD;
  size_t r = addr % FBLF_HEAP_BITS_PER_WORD;
  FblfHeapWord w0 = heap[q];
  FblfHeapWord w1 = heap[q+1];

  FblfHeapWord w0_bits = w0 << r;
  FblfHeapWord w1_bits = w1 >> (FBLF_HEAP_BITS_PER_WORD - r);
  return (w0_bits | w1_bits) >> (FBLF_HEAP_BITS_PER_WORD - n);
}

// See documentation of FblfHeapWrite in heap.h
void FblfHeapWrite(FblfHeap* heap, FblfHeapAddr addr, FblfHeapWord data, size_t n)
{
  assert(n <= FBLF_HEAP_BITS_PER_WORD && "Invalid argument");
  size_t unused_data_bits = FBLF_HEAP_BITS_PER_WORD - n;
  size_t q = addr / FBLF_HEAP_BITS_PER_WORD;
  size_t r = addr % FBLF_HEAP_BITS_PER_WORD;
  FblfHeapWord mask = ~((FblfHeapWord)0);

  // Extract, align, and overwrite the bits of data that go in word 0.
  FblfHeapWord w0 = heap[q];
  FblfHeapWord v0 = data << unused_data_bits;
  FblfHeapWord m0 = mask << unused_data_bits;
  v0 = v0 >> r;
  m0 = m0 >> r;
  heap[q] = (w0 & (~m0)) | v0;

  // Extract, align, and overwrite the bits of data that go in word 1.
  size_t non_w1_bits = unused_data_bits + (FBLF_HEAP_BITS_PER_WORD - r);
  FblfHeapWord w1 = heap[q+1];
  FblfHeapWord v1 = data << non_w1_bits;
  FblfHeapWord m1 = mask << non_w1_bits;
  heap[q+1] = (w1 & (~m1)) | v1;
}

// See documentation of FblfHeapCopy in heap.h
void FblfHeapCopy(FblfHeap* heap, FblfHeapAddr dest, FblfHeapAddr src, size_t n)
{
  while (n > FBLF_HEAP_BITS_PER_WORD) {
    FblfHeapWrite(heap, dest, FblfHeapRead(heap, src, FBLF_HEAP_BITS_PER_WORD), FBLF_HEAP_BITS_PER_WORD);
    dest += FBLF_HEAP_BITS_PER_WORD;
    src += FBLF_HEAP_BITS_PER_WORD;
    n -= FBLF_HEAP_BITS_PER_WORD;
  }
  FblfHeapWrite(heap, dest, FblfHeapRead(heap, src, n), n);
}

// See documentation of FblfHeapEquals in heap.h
bool FblfHeapEquals(FblfHeap* heap, FblfHeapAddr a, FblfHeapWord b, size_t n)
{
  return FblfGetBits(heap, a, n) == b;
}

// See documentation of FblfHeapEqual in heap.h
bool FblfHeapEqual(FblfHeap* heap, FblfHeapAddr a, FblfHeapAddr b, size_t n)
{
  while (n > FBLF_HEAP_BITS_PER_WORD) {
    if (!FblfHeapEquals(heap, a, FblfHeapRead(heap, b, FBLF_HEAP_BITS_PER_WORD), FBLF_HEAP_BITS_PER_WORD)) {
      return false;
    }
    a += FBLF_HEAP_BITS_PER_WORD;
    b += FBLF_HEAP_BITS_PER_WORD;
    n -= FBLF_HEAP_BITS_PER_WORD;
  }
  return FblfHeapEquals(heap, a, FblfHeapRead(heap, b, n), n);
}
