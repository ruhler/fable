
#include <assert.h> // for assert

#include "bits.h"

int main()
{
  { // Test basic bit equality
    FblfBits* a = FblfNewBitsFromBinary("1001101110");
    FblfBits* b = FblfNewBitsFromBinary("10101100101100");

    assert(!FblfBitsEqual(FblfGetBitRef(a, 2), FblfGetBitRef(b, 2), 4));
    assert(FblfBitsEqual(FblfGetBitRef(a, 2), FblfGetBitRef(b, 3), 4));
  
    FblfFreeBits(a);
    FblfFreeBits(b);
  }

  { // Test basic bit copy
    FblfBits* a = FblfNewBitsFromBinary("10011011110111101110");
    FblfBits* b = FblfNewBitsFromBinary("10101100101100101010");
    FblfBits* expected = FblfNewBitsFromBinary("10100101100111101110");

    FblfCopyBits(FblfGetBitRef(a, 2), FblfGetBitRef(b, 5), 9);
    assert(FblfBitsEqual(FblfGetBitRef(a, 0), FblfGetBitRef(expected, 0), 20));
  
    FblfFreeBits(a);
    FblfFreeBits(b);
    FblfFreeBits(expected);
  }

  return 0;
}
