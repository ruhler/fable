/**
 * @file fble-link.h
 *  Routines for loading fble programs.
 */

#ifndef FBLE_LINK_H_
#define FBLE_LINK_H_

#include "fble-program.h"
#include "fble-profile.h"
#include "fble-value.h"

/**
 * @func[FbleLink] Links together modules from a program into an FbleValue*.
 *  @arg[FbleValueHeap*] heap
 *   Heap to use for allocations.
 *  @arg[FbleProfile*] profile
 *   Profile to populate with blocks. May be NULL.
 *  @arg[FbleProgram*] program
 *   The program of modules to link together.
 *
 *  @returns FbleValue*
 *   A zero-argument fble function that computes the value of the program when
 *   executed, or NULL in case of error.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
FbleValue* FbleLink(FbleValueHeap* heap, FbleProfile* profile, FbleProgram* program);

#endif // FBLE_LINK_H_
