/**
 * @file fble-usage.h
 *  Helper function for finding and printing usage documentation.
 */

#ifndef FBLE_USAGE_H_
#define FBLE_USAGE_H_

/**
 * @func[FblePrintUsageDoc] Helper function to find and print usage info.
 *  Usage info is assumed to be in a file with the given @a[name]. It will be
 *  searched for in the directory of the command called as passed via @a[arg0]
 *  and in the fble doc directoy.
 *  
 *  @arg[const char*] arg0
 *   Path to the program being executed.
 *  @arg[const char*] name
 *   File name of the usage info for the program.
 *  
 *  @sideeffects
 *   Prints usage info to standard output.
 */
void FblePrintUsageDoc(const char* arg0, const char* name);

#endif // FBLE_USAGE_H_
