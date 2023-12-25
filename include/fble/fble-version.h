/**
 * @file fble-version.h
 *  Fble version info.
 */
#ifndef FBLE_VERSION_H_
#define FBLE_VERSION_H_

#include <stdio.h>    // for FILE

/**
 * Major version number.
 *
 * Major version "0" means no compatibility gaurantees of any sort from
 * release to release.
 */
#define FBLE_VERSION_MAJOR 0

/**
 * Minor version number.
 */
#define FBLE_VERSION_MINOR 3

/**
 * Version string.
 */
#define FBLE_VERSION "fble-0.3"

/**
 * A brief description with some info about the particular build of the fble
 * library.
 *
 * For debug purposes only.
 */
extern const char* FbleBuildStamp;

/**
 * @func[FblePrintVersion] Prints version info.
 *  For example, prints something like:
 *
 *  @code[txt] @
 *   fble-test fble-0.1 (2023-02-26,dev:4856517f-dirty)
 *
 *  @arg[FILE*] stream
 *   The output stream to print to.
 *  @arg[const char*] tool
 *   The name of the tool to print version for, or NULL.
 *
 *  @sideeffects
 *   Outputs version info to the given stream.
 */
void FblePrintVersion(FILE* stream, const char* tool);

#endif // FBLE_VERSION_H_
