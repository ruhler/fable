/**
 * @file literal.c
 *  Routines for encoding and decoding literal values.
 */

#include "literal.h"

#include <string.h>   // for strlen, strncmp

#include <fble/fble-vector.h>   // for FbleInitVector, FbleFreeVector

#include "tc.h"       // for FbleTagWidth

// The encoding for literal values is a mini program.
//
// The program constructs a literal starting with a unit value and empty
// list, then executing words from the program in reverse order as follows:
//
// If the word is -1, prepends the current value to the list and sets the
// current value to unit. Otherwise the word is treated as the tag width of
// a union value, the next word is treated as the tag value of a union
// value, and the value is set to the union value with given tag width, tag
// value, and previous value as the argument.

typedef struct {
  size_t size;
  size_t* xs;
} FbleTagV;

/**
 * @struct[TypeList] Linked list of types to detect recursion.
 *  @field[FbleType*][type] The current element.
 *  @field[TypeList*][next] The rest of the elements.
 */
typedef struct TypeList {
  FbleType* type;
  struct TypeList* next;
} TypeList;

static size_t GetNextLetter(FbleTypeHeap* th, FbleType* type, const char* word, FbleTagV* letter, TypeList* tl);

/**
 * @func[GetNextLetter] Finds the next letter value for a literal.
 *  @arg[FbleTypeHeap*][th] The type heap
 *  @arg[FbleType*][type] The letter type.
 *  @arg[const char*][word] The literal word to get the next letter in.
 *  @arg[FbleTagV*][letter]
 *   Output where the matched letter will be placed. This is interpreted as a
 *   sequence of (tagwidth, tag) pairs corresponding to a FbleNewLiteralValue
 *   prgm. For example, if the runtime value for the letter to construct
 *   is @code[txt]{6@(3: 2@(1: @()))}, the sequence @code[txt]{2, 1, 6, 3}.
 *  @arg[TypeList*][tl] List of type args on stack to detect recursion
 *  @returns[size_t]
 *   The number of chars in word that were matched. 0 if no match was found.
 *  @sideeffects Appends tags to letter if a match was found.
 */
static size_t GetNextLetter(FbleTypeHeap* th, FbleType* type, const char* word, FbleTagV* letter, TypeList* tl)
{
  FbleDataType* dt = (FbleDataType*)FbleNormalType(th, type);
  if (dt->_base.tag != FBLE_DATA_TYPE || dt->datatype != FBLE_UNION_DATATYPE) {
    // Letters can only be found in union types.
    FbleReleaseType(th, &dt->_base);
    return 0;
  }

  for (TypeList* l = tl; l != NULL; l = l->next) {
    if (&dt->_base == l->type) {
      // We've circled back recursively to a type we've already started
      // searching in. There's nothing new we'll find this time around.
      FbleReleaseType(th, &dt->_base);
      return 0;
    }
  }

  for (size_t i = 0; i < dt->fields.size; ++i) {
    if (FbleIsUnitType(th, dt->fields.xs[i].type)) {
      // See if we found our letter.
      const char* fieldname = dt->fields.xs[i].name.name->str;
      size_t len = strlen(fieldname);
      if (len > 0 && strncmp(word, fieldname, len) == 0) {
        FbleAppendToVector(*letter, FbleTagWidth(dt->fields.size));
        FbleAppendToVector(*letter, i);
        FbleReleaseType(th, &dt->_base);
        return len;
      }
    }

    TypeList ntl = { .type = &dt->_base, .next = tl };
    size_t sub = GetNextLetter(th, dt->fields.xs[i].type, word, letter, &ntl);

    if (sub > 0) {
      FbleAppendToVector(*letter, FbleTagWidth(dt->fields.size));
      FbleAppendToVector(*letter, i);
      FbleReleaseType(th, &dt->_base);
      return sub;
    }
  }

  FbleReleaseType(th, &dt->_base);
  return 0;
}

FbleLiteral FbleParseLiteral(FbleTypeHeap* th, FbleType* type, const char* word)
{
  const char* start = word;
  size_t wordlen = strlen(word);

  FbleTagV data;
  FbleInitVector(data);

  FbleTagV letter;
  FbleInitVector(letter);

  while (wordlen > 0) {
    letter.size = 0;
    size_t len = GetNextLetter(th, type, word, &letter, NULL);

    if (len == 0) {
      FbleFreeVector(data);
      FbleFreeVector(letter);
      FbleLiteral failed = { .size = word - start, .data = NULL };
      return failed;
    }
    
    FbleAppendToVector(data, -1);
    for (size_t i = 0; i < letter.size; ++i) {
      size_t tag = letter.xs[letter.size - i - 1];
      FbleAppendToVector(data, tag);
    }

    word += len;
    wordlen -= len;
  }

  FbleFreeVector(letter);

  FbleLiteral literal = { .size = data.size, .data = data.xs };
  return literal;
}

// See documentation in fble-literal.h.
FbleValue* FbleNewLiteralValue(FbleValueHeap* heap, size_t size, size_t* data)
{
  FbleValue* unit = FbleNewStructValue_(heap, 0);
  FbleValue* tail = FbleNewUnionValue(heap, 1, 1, unit);
  FbleValue* letter = unit;
  for (size_t* pc = data + size - 1; pc >= data; --pc) {
    if (*pc == -1) {
      FbleValue* cons = FbleNewStructValue_(heap, 2, letter, tail);
      tail = FbleNewUnionValue(heap, 1, 0, cons);
    } else {
      size_t tagwidth = *pc;
      pc--;
      size_t tag = *pc;
      letter = FbleNewUnionValue(heap, tagwidth, tag, letter);
    }
  }
  return tail;
}
