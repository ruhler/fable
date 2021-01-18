#ifndef FBLF_BITS_H_
#define FBLF_BITS_H_

#include <stdbool.h>    // for bool
#include <stdint.h>     // for uint64_t

// FblfBitsWord --
//   The type of word used in a bits sequence.
typedef uint64_t FblfBitsWord;

// FblfBits --
//   A type representing a sequence of raw bits.
//
// FblfBits should be an array of FblfBitsWord words. The sequence of bits is
// densly packed into consecutive words of the array, with earlier bits in the
// sequence being packed in earlier words of the array, most significant bit
// to least significant bit within a word.
//
// The least significant bits of the last word of the array will be unused if
// the number of bits is not a multiple of the word size.
typedef FblfBitsWord* FblfBits;

// FblfBitIndex --
//   A number identifying a particular bit in an FblfBits sequence. The first
//   bit in the sequence has index 0, the second has index 1, and so on.
typedef size_t FblfBitIndex;

// FblfCopyBits --
//   Copies count bits from the src bit sequence starting at src_index to the
//   destination bit sequence starting at dest_index.
//
// Inputs:
//   dest - the destination sequence.
//   dest_index - the first bit in the destination sequence to copy from the src.
//   src - the source sequence.
//   source_index - the first bit in the source sequence to copy to the dst.
//   count - the number of bits to copy.
//
// Side effects:
//   Copies count bits from the src bit sequence starting at src_index to the
//   destination bit sequence starting at dest_index.
//
//   Behavior is undefined if there are insufficient bits in the source or
//   destination sequence to copy count of.
//
//   The source and destination ranges may overlap.
void FblfCopyBits(FblfBits dest, FblfBitIndex dest_index, FblfBitIndex src, FblfBitIndex src_index, size_t count);

// FblfBitsEqual --
//   Compare parts of two bit sequences.
//
// Inputs:
//   a - the first bit sequence.
//   a_index - the first bit in the first sequence to start comparing from.
//   b - the second bit sequence.
//   b_index - the first bit in the second sequence to start comparing from.
//   count - the number of bits to compare.
//
// Returns:
//   true if the bits being compared are equal in both sequences, false
//   otherwise.
//
// Side effects:
//   Behavior is undefined if there are insufficient bits in the source or
//   destination sequence to compare count of.
bool FblfBitsEqual(FblfBits dest, FblfBitIndex dest_index, FblfBits src, FblfBitIndex src_index, size_t count);

#endif // FBLF_BITS_H_
