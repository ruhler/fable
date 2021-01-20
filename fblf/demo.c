
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

typedef enum {
  BIT_0, BIT_1
} Bit;

typedef struct {
  Bit b3;
  Bit b2;
  Bit b1;
  Bit b0;
} Bit4;

typedef struct {
  Bit b3 : 1;
  Bit b2 : 1;
  Bit b1 : 1;
  Bit b0 : 1;
} BitfieldBit4;

typedef struct {
  Bit b3;
  Bit b2;
  Bit b1;
  Bit b0;
} __attribute__ ((__packed__)) PackedBit4;

typedef struct {
  unsigned int b3 : 1;
  unsigned int b2 : 1;
  unsigned int b1 : 1;
  unsigned int b0 : 1;
} NonEnumBitFieldBit4;

typedef struct {
  Bit is_valid;
  Bit4 data;
} Maybe;

typedef union {
  Bit bit;
  Maybe maybe;
} U;

// A thread that counts up using a loop:
// Count:
//   s = 0;
//   while (s < 100) {
//     put(s);
//     s++;
//   }
//
// Has three entry points:
// 1. the first statement
// 2. the while loop (perhaps this can be omitted because there's a yield in
// the body for the put already?)
// 3. the put, in case it blocks.
typedef int PC;
static PC Count(const char* prefix, int* s, PC pc)
{
  switch (pc) {
    case 0:
      *s = 0;
    case 1:
      while (*s < 100) {
    case 2:
        if (rand() % 3 == 0) {
          // yield if put is blocked.
          return 2;
        }
        printf("%s %i\n", prefix, *s);

        (*s)++;

        if (rand() % 10 == 0) {
          // yield to avoid starvation back to top of the loop.
          return 1;
        }
      }
  }
  return 3;
}

int main()
{
  // Initialization of a complex data type.
  U u;
  u.maybe.is_valid = BIT_1;
  u.maybe.data.b3 = BIT_0;
  u.maybe.data.b2 = BIT_1;
  u.maybe.data.b1 = BIT_0;
  u.maybe.data.b0 = BIT_1;

  // Assignment of a complex data type.
  U u2;
  u2 = u;

  // Equality of a complex data type.
  //   Default equality of union not supported.
  //   Default equality of struct not supported.
  //   Default equality of enum is supported.
  bool eq = (u.maybe.is_valid == u2.maybe.is_valid);

  // Bitfield packing
  printf("sizeof Bit: %i\n", sizeof(Bit));
  printf("sizeof Bit4: %i\n", sizeof(Bit4));
  printf("sizeof BitfieldBit4: %i\n", sizeof(BitfieldBit4));
  printf("sizeof PackedBit4: %i\n", sizeof(PackedBit4));
  printf("sizeof NonEnumBitFieldBit4: %i\n", sizeof(NonEnumBitFieldBit4));

  // Demonstration of two interleaved threads.
  int s1, s2;
  PC pc1 = 0;
  PC pc2 = 0;
  do {
    pc1 = Count("C1", &s1, pc1);
    pc2 = Count("C2", &s2, pc2);
  } while (pc1 < 3 || pc2 < 3);

  return eq ? 0 : 1;
}
