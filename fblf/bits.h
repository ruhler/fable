#ifndef FBLF_BITS_H_
#define FBLF_BITS_H_

#include <stdbool.h>      // for bool
#include <sys/types.h>    // for size_t

// FblfBits --
//   A type representing a sequence of bits.
typedef struct FblfBits FblfBits;

// FblfNewBits --
//   Allocate a new sequence of bits of given length.
//
// Inputs:
//   n - the number of bits in the sequence.
//
// Results:
//   A newly allocated, uninitialized bit sequence of n bits.
//
// Side effects:
//   The user must call FblfFreeBits when the bit sequence is no longer in
//   use.
FblfBits* FblfNewBits(size_t n);

// FblfNewBitsFromBinary --
//   Allocate a new sequence of bits initialized with a sequence of binary
//   digits.
//
// Inputs:
//   bitstr - The sequence of binary digits to populate the bit sequence with.
//
// Results:
//   A newly allocated sequence with 1 bits for each binary digit in the input
//   sequence.
//
// Side effects:
// * Behavior is undefined if not all characters in bitstr are binary digits.
// * The user must call FblfFreeBits when the bit sequence is no longer in
//   use.
//
// Example:
//   FblfNewBitsFromBinary("01001010110") creates a sequence of 11 bits: 01001010110
FblfBits* FblfNewBitsFromBinary(const char* bitstr);

// FblfFreeBits --
//   Free resources associated with FblfBits.
//
// Inputs:
//   bits - the FblfBits to free resources for.
//
// Side effects:
//   Frees resources associated with the given FblfBits.
void FblfFreeBits(FblfBits* bits);

// FblfBitRef --
//   A reference to the i'th bit of the sequence of bits.
typedef struct {
  FblfBits* bits;
  size_t i;
} FblfBitRef;

// FblfGetBitRef --
//   Gets a reference to the i'th bit in the given bits.
//
// Inputs:
//   bits - the bit sequence to get a reference into.
//   i - the bit of the sequence to get a reference for.
//
// Returns:
//   A reference to the i'th bit of bits.
//
// Side effects:
//   None.
FblfBitRef FblfGetBitRef(FblfBits* bits, size_t i);

// FblfCopyBits --
//   Copies count bits starting from src to the given destination.
//
// Inputs:
//   dest - the destination sequence.
//   src - the source sequence.
//   count - the number of bits to copy.
//
// Side effects:
//   Copies count bits from src to dest.
//
//   Behavior is undefined if there are insufficient bits in the source or
//   destination sequence to copy count of.
//
//   The source and destination ranges may overlap.
void FblfCopyBits(FblfBitRef dest, FblfBitRef src, size_t count);

// FblfBitsEqual --
//   Compare parts of two bit sequences.
//
// Inputs:
//   a - the first bit sequence.
//   b - the second bit sequence.
//   count - the number of bits to compare.
//
// Returns:
//   true if the bits being compared are equal in both sequences, false
//   otherwise.
//
// Side effects:
//   Behavior is undefined if there are insufficient bits in the source or
//   destination sequence to compare count of.
bool FblfBitsEqual(FblfBitRef a, FblfBitRef b, size_t count);

#endif // FBLF_BITS_H_
