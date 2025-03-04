/**
 * @file fble-generate.h
 *  Fble backend code generation API.
 */

#ifndef FBLE_GENERATE_H_
#define FBLE_GENERATE_H_

#include <stdio.h>        // for FILE

#include "fble-program.h"   // for FbleModule

/**
 * @func[FbleGenerateAArch64] Generates aarch64 for a compiled module.
 *  The generated code exports an FblePreloadedModule named based on the
 *  module path.
 *  
 *  @arg[FILE*] fout
 *   The output stream to write the C code to.
 *  @arg[FbleModule*] module
 *   The module to generate code for.
 *
 *  @sideeffects
 *   Generates aarch64 code for the given module.
 */
void FbleGenerateAArch64(FILE* fout, FbleModule* module);

/**
 * @func[FbleGenerateAArch64Export] Generates aarch64 to export a compiled module.
 *  The generated code will export an FblePreloadedModule* with the given
 *  name.
 *
 *  @arg[FILE*] fout
 *   The output stream to write the C code to.
 *  @arg[const char*] name
 *   The name of the FblePreloadedModule* to generate.
 *  @arg[FbleModulePath*] path
 *   The path to the module to export.
 *
 *  @sideeffects
 *   Outputs aarch64 code to the given file.
 */
void FbleGenerateAArch64Export(FILE* fout, const char* name, FbleModulePath* path);

/**
 * @func[FbleGenerateAArch64Main] Generates aarch64 code for main.
 *  Generate aarch64 code for a main function that invokes an
 *  FblePreloadedModule* with the given wrapper function.
 *
 *  The generated code will export a main function of the following form:
 *
 *  @code[c] @
 *   int main(int argc, const char** argv) {
 *     return _main_(argc, argv, _compiled_);
 *   }
 *
 *  Where _compiled_ is the FblePreloadedModule* corresponding to the given
 *  module path.
 *
 *  @arg[FILE*] fout
 *   The output stream to write the C code to.
 *  @arg[const char*] main
 *   The name of the wrapper function to invoke.
 *  @arg[FbleModulePath*] path
 *   The path to the module to pass to the wrapper function.
 *
 *  @sideeffects
 *   Generates aarch64 code for the given code.
 */
void FbleGenerateAArch64Main(FILE* fout, const char* main, FbleModulePath* path);

/**
 * @func[FbleGenerateC] Generates C code for a compiled module.
 *  The generated code exports an FblePreloadedModule named based on the
 *  module path.
 *
 *  @arg[FILE*] fout
 *   The output stream to write the C code to.
 *  @arg[FbleModule*] module
 *   The module to generate code for.
 *
 *  @sideeffects
 *   Outputs C code to the given file.
 */
void FbleGenerateC(FILE* fout, FbleModule* module);

/**
 * @func[FbleGenerateCExport] Generates C code to export a compiled module.
 *  The generated code will export an FblePreloadedModule* with the given
 *  name.
 *
 *  @arg[FILE*] fout
 *   The output stream to write the C code to.
 *  @arg[const char*] name
 *   The name of the function to generate.
 *  @arg[FbleModulePath*] path
 *   The path to the module to export.
 *  
 *  @sideeffects
 *   Outputs C code to the given file.
 */
void FbleGenerateCExport(FILE* fout, const char* name, FbleModulePath* path);

/**
 * @func[FbleGenerateCMain] Generates C code for main.
 *  Generate C code for a main function that invokes an FblePreloadedModule*
 *  with the given wrapper function.
 *  The generated code will export a main function of the following form:
 *  
 *  @code[c] @
 *   int main(int argc, const char** argv) {
 *     return _main_(argc, argv, _compiled_);
 *   }
 *
 *  Where _compiled_ is the FblePreloadedModule* corresponding to the given
 *  module path.
 *
 *  @arg[FILE*] fout
 *   The output stream to write the C code to.
 *  @arg[const char*] main
 *   The name of the wrapper function to invoke.
 *  @arg[FbleModulePath*] path
 *   The path to the module to pass to the wrapper function.
 *  
 *  @sideeffects
 *   Outputs C code to the given file.
 */
void FbleGenerateCMain(FILE* fout, const char* main, FbleModulePath* path);

#endif // FBLE_GENERATE_H_
