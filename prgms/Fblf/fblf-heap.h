
#ifndef FBLF_HEAP_H_
#define FBLF_HEAP_H_

#include <stdbool.h>      // for bool
#include <stdint.h>       // for uint64_t
#include <sys/types.h>    // for size_t

// FblfHeapWord --
//   The word size used for conveniently storing and transfering a relatively
//   small number of packed bits.
typedef uint64_t FblfHeapWord;

// FblfHeap --
//   FblfHeap* represents an fblf heap, which is an array of heap words.
//
// Typically this is assumed to have a sufficient number of words to hold all
// the bits needed for whatever operations are performed on the heap.
typedef FblfHeapWord FblfHeap;

// FblfHeapAddr --
//   Used to identify a particular bit within an fblf heap.
//
// Bits start at address 0. The address increases by one for each subsequent
// bit. Bits are packed into consecutive words, most significant bit first.
typedef size_t FblfHeapAddr;

// FBLF_HEAP_BITS_PER_WORD --
//   The number of bits that fits into an FblfHeapWord.
#define FBLF_HEAP_BITS_PER_WORD 64

// The number of FBLF_HEAP_WORDS needed to store the given number of bits.
#define FBLF_HEAP_WORDS_FOR_BITS(bits) ((bits + FBLF_HEAP_BITS_PER_WORD - 1) / FBLF_HEAP_BITS_PER_WORD)

// FblfHeapRead --
//   Read n bits from the heap at the given bit address.
//
// Inputs:
//   heap - the heap to read from.
//   addr - the address of the first bit of the given heap to read.
//   n - the number of bits to read. Must be no more than
//       FBLF_HEAP_BITS_PER_WORD.
//
// Results:
//   The least significant bits of the returned word contain the bits read
//   from the sequence in order of most significant to least significant bit.
//   The remaining most significant bits of the returned word, if any, will be
//   0.
//
// Side effects:
// * Behavior is undefined if the range of bits read does not fit within an
//   allocated sequence of bits.
// * Behavior is undefined if n > FBLF_HEAP_BITS_PER_WORD.
FblfHeapWord FblfHeapRead(FblfHeap* heap, FblfHeapAddr addr, size_t n);

// FblfHeapWrite --
//   Write n bits of data to the heap at the given bit address.
//
// Inputs:
//   heap - the heap to write to.
//   addr - the address of the first bit of the given heap to write.
//   data - the data to write, packed into the least significant n bits of the
//          word in order from most significant bit to least significant bit. 
//          The remaining unused most significant bits of the value must be
//          set to 0.
//   n - the number of bits to write. Must be no more than
//       FBLF_HEAP_BITS_PER_WORD.
//
// Side effects:
// * Sets the value of the n bits starting at the given address.
// * Behavior is undefined if the range of bits read does not fit within an
//   allocated sequence of bits.
// * Behavior is undefined if n > 64.
// * Behavior is undefined if the unused bits of value are not set to 0.
void FblfHeapWrite(FblfHeap* heap, FblfHeapAddr addr, FblfHeapWord data, size_t n);

// FblfHeapCopy --
//   Copy n bits in the given heap from the src location to the dest location.
//
// Inputs:
//   heap - the heap to make the copy in.
//   dest - the address of the first bit to be overwritten with the copied
//          data.
//   src - the address of the first bit with the values to be read from.
//   n - the number of bits to copy.
//
// TODO: What if the src and dest ranges overlap?
//
// Side effects:
// * Sets the value of the n bits starting at the given dest.
// * Behavior is undefined if the range of bits read or written does not fit
//   within an allocated sequence of bits.
void FblfHeapCopy(FblfHeap* heap, FblfHeapAddr dest, FblfHeapAddr src, size_t n);

// FblfHeapEquals --
//   Test whether the n bits at address a of the heap match bits in word b.
//
// Inputs:
//   heap - the heap containing the bits to check.
//   a - the address of the first bit in the sequence to check.
//   b - the contents to check the bit sequence against, packed as in
//       FblfHeapWrite.
//   n - the number of bits to check.
//
// Results:
//   true if the bits are equal, false otherwise.
//
// Side effects:
//   None.
bool FblfHeapEquals(FblfHeap* heap, FblfHeapAddr a, FblfHeapWord b, size_t n);

// FblfHeapEqual --
//   Test whether the n bits at address a of the heap match the bits at
//   address b in the heap.
//
// Inputs:
//   heap - the heap containing the bits to check.
//   a - the address of the first bit in the sequence a to check.
//   b - the address of the first bit in the sequence b to check.
//   n - the number of bits to check.
//
// Results:
//   true if the bits are equal, false otherwise.
//
// Side effects:
//   None.
bool FblfHeapEqual(FblfHeap* heap, FblfHeapAddr a, FblfHeapAddr b, size_t n);

#endif // FBLF_HEAP_H_
