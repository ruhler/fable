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
      FbleArgParser
      FbleParseBoolArg FbleParseStringArg
      FbleModuleArg FbleNewModuleArg FbleFreeModuleArg FbleParseModuleArg
      FbleParseInvalidArg
    }
    fble-compile.h {
      FbleCompileModule FbleCompileProgram
      FbleDisassemble
    }
    fble-function.h {
      FbleExecutable FbleFunction
      FbleRunFunction
      FbleCall FbleTailCall
    }
    fble-generate.h {
      FbleGenerateAArch64 FbleGenerateAArch64Export FbleGenerateAArch64Main
      FbleGenerateC FbleGenerateCExport FbleGenerateCMain
    }
    fble-link.h {
      FbleLink
    }
    fble-load.h {
      FbleParse
      FbleNewSearchPath FbleFreeSearchPath
      FbleAppendToSearchPath FbleAppendStringToSearchPath
      FbleFindPackage
      FbleLoadForExecution FbleLoadForModuleCompilation
      FbleSaveBuildDeps
    }
    fble-loc.h {
      FbleLoc FbleLocV
      FbleNewLoc FbleCopyLoc FbleFreeLoc
      FbleReportWarning FbleReportError
    }
    fble-main.h {
      FbleMain FbleMainStatus
    }
    fble-module-path.h {
      FbleModulePathMagic FbleModulePath FbleModulePathV
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
      FbleNameSpace FbleName FbleNameV
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
    fble-program.h {
      FbleModule FbleModuleV FbleProgram
      FbleFreeModule FbleFreeProgram
      FblePreloadedModule FblePreloadedModuleV
    }
    fble-string.h {
      FbleStringMagic FbleString FbleStringV
      FbleNewString FbleCopyString FbleFreeString
    }
    fble-value.h {
      FbleValue FbleValueHeap
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
      FbleDeclareRecursiveValues FbleDefineRecursiveValues
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
