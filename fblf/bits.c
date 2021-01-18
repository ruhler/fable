
#include "bits.h"

typedef enum {
  BIT_0, BIT_1
} Bit;

static Bit GetBit(FblfBits bits, FblfBitIndex index);
static void SetBit(FblfBits bits, FblfBitIndex index, Bit value);


// GetBit --
//   Get the bit at the given index of the given bit sequence.
//
// Inputs:
//   bits - the bit sequence to get from.
//   index - the index of the bit to get.
//
// Returns: 
//   The bit ad the given index of the bit sequence.
//
// Side effects:
//   Behavior is undefined if the index is not in range of the bit sequence.
static Bit GetBit(FblfBits bits, FblfBitIndex index)
{
  FblfBitsWord word = bits[index / sizeof(FblfBitsWord)];
  word >>= (sizeof(FblfBitsWord) - (index % sizeof(FblfBitsWord)) - 1);
  return (word & 0x1 == 0) ? BIT_0 : BIT_1;
}

// SetBit --
//   Set the bit at the given index of the given bit sequence to the given
//   value.
//
// Inputs:
//   bits - the bit sequence to set a bit in from.
//   index - the index of the bit to set.
//   value - the value to set the bit to.
//
// Side effects:
//   Behavior is undefined if the index is not in range of the bit sequence.
static void SetBit(FblfBits bits, FblfBitIndex index, Bit value)
{
  FblfBitsWord bit = 1 << (sizeof(FblfBitsWord) - (index % sizeof(FblfBitsWord)) - 1);
  bits[index / sizeof(FblfBitsWord)] |= bit;
}

// FblfCopyBits -- See documentation in bits.h
void FblfCopyBits(FblfBits dest, FblfBitIndex dest_index, FblfBitIndex src, FblfBitIndex src_index, size_t count)
{
  // TODO: Make sure we properly handle the alias case.
  // TODO: can this be made more efficient by bulk comparing bits somehow?
  for (size_t i = 0; i < count; ++i) {
    SetBit(dest, dest_index+i, GetBit(src, src_index+i));
  }
}

// FblfBitsEqual -- See documentation in bits.h
bool FblfBitsEqual(FblfBits dest, FblfBitIndex dest_index, FblfBits src, FblfBitIndex src_index, size_t count)
{
  // TODO: can this be made more efficient by bulk comparing bits somehow?
  for (size_t i = 0; i < count; ++i) {
    if (GetBit(dest, dest_index + i) != GetBit(src, src_index+i)) {
      return false;
    }
  }
  return true;
}
