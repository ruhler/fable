namespace eval "include" {
  foreach {x} [build_glob $::s/include/fble *.h] {
    install $x $::config::includedir/fble/[file tail $x]
  }

  set header_funcs {
    fble-alloc.h {
      FbleAllocRaw FbleReAllocRaw
      FbleAlloc FbleAllocExtra
      FbleAllocArray FbleReAllocArray
      FbleFree
      FbleMaxTotalBytesAllocated FbleResetMaxTotalBytesAllocated
    }
    fble-arg-parse.h {
      FbleParseBoolArg FbleParseStringArg
      FbleModuleArg FbleNewModuleArg FbleFreeModuleArg FbleParseModuleArg
      FbleParseInvalidArg
    }
    fble-compile.h {
      FbleCompiledModule FbleCompiledModuleV FbleCompiledProgram
      FbleFreeCompiledModule FbleFreeCompiledProgram
      FbleCompileModule FbleCompileProgram
      FbleDisassemble
    }
    fble-function.h {
      FbleExecutable FbleFunction
      FbleRunFunction
      FbleCall FbleTailCall
    }
    fble-generate.h {
      FbleGeneratedModule FbleGeneratedModuleV
      FbleGenerateAArch64 FbleGenerateAArch64Export FbleGenerateAArch64Main
      FbleGenerateC FbleGenerateCExport FbleGenerateCMain
    }
    fble-link.h {
      FbleLink
      FblePrintCompiledHeaderLine
    }
    fble-load.h {
      FbleLoadedModule FbleLoadedModuleV FbleLoadedProgram
      FbleParse
      FbleNewSearchPath FbleFreeSearchPath
      FbleAppendToSearchPath FbleAppendStringToSearchPath
      FbleFindPackage
      FbleLoad
      FbleFreeLoadedProgram
      FbleSaveBuildDeps
    }
    fble-loc.h {
      FbleLoc
      FbleNewLoc FbleCopyLoc FbleFreeLoc
      FbleReportWarning FbleReportError
    }
    fble-module-path.h {
      FbleModulePath FbleModulePathV
      FbleNewModulePath
      FbleModulePathName
      FblePrintModulePath
      FbleModulePathsEqual
      FbleModuleBelongsToPackage
      FbleParseModulePath
      FbleCopyModulePath
      FbleFreeModulePath
    }
    fble-name.h {
      FbleName FbleNameV
      FbleCopyName FbleFreeName FbleNamesEqual FblePrintName
    }
    fble-profile.h {
      FbleBlockIdV
      FbleCallData FbleCallDataV
      FbleBlockProfile FbleBlockProfileV
      FbleProfile
      FbleNewProfile
      FbleAddBlockToProfile FbleAddBlocksToProfile
      FbleFreeProfile
      FbleNewProfileThread FbleFreeProfileThread
      FbleProfileSample FbleProfileRandomSample
      FbleProfileEnterBlock FbleProfileReplaceBlock FbleProfileExitBlock
      FbleGenerateProfileReport
    }
    fble-string.h {
      FbleString FbleStringV FbleNewString FbleCopyString FbleFreeString
    }
    fble-usage.h {
      FblePrintUsageDoc
    }
    fble-value.h {
      FbleNewValueHeap FbleFreeValueHeap
      FbleValueV
      FbleGenericTypeValue
      FbleNewStructValue FbleNewStructValue_
      FbleStructValueField
      FbleNewUnionValue FbleNewEnumValue
      FbleUnionValueTag FbleUnionValueArg FbleUnionValueField
      FbleNewListValue FbleNewListValue_ FbleNewLiteralValue
      FbleNewFuncValue FbleFuncValueFunction
      FbleEval FbleApply
      FbleNewRefValue FbleAssignRefValue
    }
    fble-vector.h {
      FbleInitVector FbleFreeVector
      FbleExtendVector FbleAppendToVector
      FbleExtendVectorRaw
    }
    fble-version.h {
      FblePrintVersion
    }
  }

  foreach {x} [build_glob $::s/include/fble -tails "*.h"] {
    fbld_check_dc $::b/include/fble/$x.dc $::s/include/fble/$x
  }

  foreach {header funcs} $header_funcs {
    foreach x $funcs {
      fbld_man_dc $::b/include/fble/$x.3 $::s/include/fble/$header $x
      install $::b/include/fble/$x.3 $::config::mandir/man3/$x.3
    }
  }
}
