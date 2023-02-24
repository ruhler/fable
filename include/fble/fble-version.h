/**
 * @file fble-version.h
 * Fble version info.
 */
#ifndef FBLE_VERSION_H_
#define FBLE_VERSION_H_

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
#define FBLE_VERSION_MINOR 1

/**
 * Version string.
 */
#define FBLE_VERSION "fble-0.1"

/**
 * A brief description with some info about the particular build of the fble
 * library.
 *
 * For debug purposes only.
 */
extern const char* FbleBuildStamp;

#endif // FBLE_VERSION_H_
