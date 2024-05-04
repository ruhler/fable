/**
 * @file memory.h
 *  Header for platform specific memory API.
 */
#ifndef FBLE_TEST_MEMORY_H_
#define FBLE_TEST_MEMORY_H_

#include <stddef.h>   // for size_t

/**
 * @func[FbleGetMaxMemoryUsageKB] Gets maximum memory usage of the process.
 *  @returns[long] Max resident set size of the process in kilobytes.
 */
size_t FbleGetMaxMemoryUsageKB();

#endif // FBLE_TEST_MEMORY_H_
