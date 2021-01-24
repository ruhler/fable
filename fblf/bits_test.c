
#include <assert.h> // for assert

#include "bits.h"

int main()
{
  { // Test basic getting and setting of bit strings.
    FblfBitPtr a = FblfNewBits(20);
    FblfSetBits(a, 20, 0xd3d24);
    assert(0x0 == FblfGetBits(a, 0));
    assert(0xd == FblfGetBits(a, 4));
    assert(0x69e == FblfGetBits(a, 11));

    assert(0x0 == FblfGetBits(a+5, 0));
    assert(0x7 == FblfGetBits(a+5, 4));
    assert(0x3d2 == FblfGetBits(a+5, 11));
  }

  { // Test gets of various position and size from a binary bit string.
    FblfBitPtr a = FblfNewBitsFromBinary("1101001111010010010001010100010011001");
    assert(0x0 == FblfGetBits(a, 0));
    assert(0xd == FblfGetBits(a, 4));
    assert(0x69e == FblfGetBits(a, 11));

    assert(0x0 == FblfGetBits(a+5, 0));
    assert(0x7 == FblfGetBits(a+5, 4));
    assert(0x3d2 == FblfGetBits(a+5, 11));
  }


  { // Test gets of various position and size from a hex string.
    FblfBitPtr a = FblfNewBitsFromHex("d3d24544c");
    assert(0x0 == FblfGetBits(a, 0));
    assert(0xd == FblfGetBits(a, 4));
    assert(0x69e == FblfGetBits(a, 11));

    assert(0x0 == FblfGetBits(a+5, 0));
    assert(0x7 == FblfGetBits(a+5, 4));
    assert(0x3d2 == FblfGetBits(a+5, 11));
  }

  { // Test basic bit equality
    FblfBitPtr a = FblfNewBitsFromBinary("1001101110");
    FblfBitPtr b = FblfNewBitsFromBinary("10101100101100");

    assert(!FblfBitsEqual(a + 2, b + 2, 4));
    assert(FblfBitsEqual(a + 2, b + 3, 4));
  
    FblfFreeBits(a);
    FblfFreeBits(b);
  }

  { // Test basic bit copy
    FblfBitPtr a = FblfNewBitsFromBinary("10011011110111101110");
    FblfBitPtr b = FblfNewBitsFromBinary("10101100101100101010");
    FblfBitPtr expected = FblfNewBitsFromBinary("10100101100111101110");

    FblfCopyBits(b + 5, a + 2, 9);
    assert(FblfBitsEqual(a, expected, 20));
  
    FblfFreeBits(a);
    FblfFreeBits(b);
    FblfFreeBits(expected);
  }

  return 0;
}
