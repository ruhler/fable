#ifndef FBLF_BITS_H_
#define FBLF_BITS_H_

#include <stdbool.h>      // for bool
#include <stdint.h>       // for uint64_t
#include <sys/types.h>    // for size_t

// FblfBitPtr --
//   A pointer into a sequence of allocated bits. Arithmetic operations are
//   supported.
typedef uint64_t FblfBitPtr;

// FblfNewBits --
//   Allocate a new sequence of n bits.
//
// Inputs:
//   n - the number of bits in the sequence to allocate.
//
// Results:
//   A pointer to the allocated bits.
//
// Side effects:
// * Allocates n uninitialized bits that should be freed using FblfFreeBits
//   when no longer needed.
// * Behavior is undefined if there is insufficient memory to allocate the new
//   bit sequence.
FblfBitPtr FblfNewBits(size_t n);

// FblfNewBitsFromBinary --
//   Allocate a new sequence of bits initialized with a sequence of binary
//   digits.
//
// Inputs:
//   binstr - The sequence of binary digits to populate the bit sequence with.
//
// Results:
//   A newly allocated sequence with 1 bits for each binary digit in the input
//   sequence.
//
// Side effects:
// * Behavior is undefined if not all characters in binstr are binary digits.
// * The user must call FblfFreeBits when the bit sequence is no longer in
//   use.
//
// Example:
//   FblfNewBitsFromBinary("01001010110") creates a sequence of 11 bits: 01001010110
FblfBitPtr FblfNewBitsFromBinary(const char* binstr);

// FblfNewBitsFromHex --
//   Allocate a new sequence of bits initialized with a sequence of hex
//   digits.
//
// Inputs:
//   hexstr - The sequence of hex digits to populate the bit sequence with.
//            Capital and lowercase hex digits are accepted.
//
// Results:
//   A newly allocated sequence with 4 bits for each hex digit in the input
//   sequence.
//
// Side effects:
// * Behavior is undefined if not all characters in hexstr are hex digits.
// * The user must call FblfFreeBits when the bit sequence is no longer in
//   use.
//
// Example:
//   FblfNewBitsFromHex("a3b") creates a sequence of 12 bits: 101000111011
FblfBitPtr FblfNewBitsFromHex(const char* hexstr);

// FblfFreeBits --
//   Free a bit sequence.
//
// Inputs:
//   ptr - A pointer to the bit sequence to free.
//
// Side effects:
// * Makes the bits pointed to by ptr available for new bit sequence
//   allocations.
// * Behavior is undefined if ptr does not refer to the start of a bit
//   sequence returned by FblfNewBits.
// * The number of bits freed is the number of bits originally allocated for
//   the sequence.
void FblfFreeBits(FblfBitPtr ptr);

// FblfGetBits --
//   Read n bits starting at the given bit address.
//
// Inputs:
//   ptr - A pointer to the first bit in the sequence to read.
//   n - The number of bits to read. Must be less than or equal to 64.
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
// * Behavior is undefined if n > 64.
uint64_t FblfGetBits(FblfBitPtr ptr, size_t n);

// FblfSetBits --
//   Write n bits to the given bit address.
//
// Inputs:
//   ptr - A pointer to the first bit in the sequence to write.
//   n - The number of bits to read. Must be less than or equal to 64.
//   value - The bits to write, specified in the least significant n bits of
//   the value in order from most significant bit to least significant bit.
//   The remaining unused most significant bits of the value must be set to 0.
//
// Side effects:
// * Sets the value of the n bits starting at ptr.
// * Behavior is undefined if the range of bits read does not fit within an
//   allocated sequence of bits.
// * Behavior is undefined if n > 64.
// * Behavior is undefined if the unused bits of value are not set to 0.
void FblfSetBits(FblfBitPtr ptr, size_t n, uint64_t value);

// FblfCopyBits --
//   Copy n bits from src to dst.
//
// Inputs:
//   src - a pointer to the bit to start copying from.
//   dst - a pointer to the bit to start copying to.
//   n - the number of consecutive bits to copy.
//
// Side effects:
// * Copies n bits from src to dst.
// * Behavior is undefined if the range of bits copied does not fit within the
//   allocated sequences of bits for src or dst.
void FblfCopyBits(FblfBitPtr src, FblfBitPtr dst, size_t n);

// FblfBitsEqual --
//   Test whether two sequence of bits are equal.
//
// TODO: Clarify whether the sequences are allowed to be overlapping or not.
//
// Inputs:
//   a - a pointer to the first bit in the first sequence to test.
//   b - a pointer to the first bit in the second sequence to test.
//   n - the number of consecutive bits to compare.
//
// Result:
//   true if the n bits of a and b are the identical. false otherwise.
//
// Side effects:
// * Behavior is undefined if the range of bits copied does not fit within the
//   allocated sequences of bits for a or b.
bool FblfBitsEqual(FblfBitPtr a, FblfBitPtr b, size_t n);

#endif // FBLF_BITS_H_
