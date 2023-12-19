/**
 * @file unreachable.h
 *  Defines FbleUnreachable macro.
 */
#ifndef FBLE_INTERNAL_UNREACHABLE_H_
#define FBLE_INTERNAL_UNREACHABLE_H_

#include <assert.h>   // for assert.
#include <stdbool.h>  // for false.

/** 
 * Indicates that a peice of code is unreachable.
 *
 * The primary motivation for this is to make it easy to see when looking at
 * code coverage reports which uncovered lines of code are expected to be
 * unreachable.
 *
 * @param x  Message explaining why the code is unreachable.
 */
#define FbleUnreachable(x) assert(false && x)

#endif // FBLE_INTERNAL_UNREACHABLE_H_
