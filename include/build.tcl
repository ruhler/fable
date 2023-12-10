namespace eval "include" {
  foreach {x} [build_glob $::s/include/fble *.h] {
    install $x $::config::includedir/fble/[file tail $x]
  }

  set header_funcs {
    fble-alloc.h {
      FbleRawAlloc FbleAlloc FbleAllocExtra FbleArrayAlloc
      FbleFree
      FbleMaxTotalBytesAllocated FbleResetMaxTotalBytesAllocated
    }
    fble-arg-parse.h {
      FbleParseBoolArg FbleParseStringArg
      FbleNewModuleArg FbleFreeModuleArg FbleParseModuleArg
      FbleParseInvalidArg
    }
    fble-compile.h {
      FbleFreeCompiledModule FbleFreeCompiledProgram
      FbleCompileModule FbleCompileProgram
      FbleDisassemble
      FbleGenerateAArch64 FbleGenerateAArch64Export FbleGenerateAArch64Main
      FbleGenerateC FbleGenerateCExport FbleGenerateCMain
    }
    fble-execute.h {
      FbleRunFunction
      FbleFreeExecutable
      FbleExecutableNothingOnFree
      FbleFreeExecutableModule FbleFreeExecutableProgram
      FbleThreadCall FbleThreadCall_
    }
    fble-interpret.h {
      FbleInterpret
    }
    fble-link.h {
      FbleLink
      FbleLinkFromSource
      FbleCompiledModuleFunction
      FbleLoadFromCompiled
      FbleLinkFromCompiled
      FbleLinkFromCompiledOrSource
      FblePrintCompiledHeaderLine
    }
    fble-load.h {
      FbleParse
      FbleNewSearchPath FbleFreeSearchPath
      FbleSearchPathAppend FbleSearchPathAppendString
      FbleFindPackage
      FbleLoad
      FbleFreeLoadedProgram
      FbleSaveBuildDeps
    }
    fble-loc.h {
      FbleNewLoc FbleCopyLoc FbleFreeLoc
      FbleReportWarning FbleReportError
    }
    fble-module-path.h {
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
      FbleCopyName FbleFreeName FbleNamesEqual FblePrintName
    }
    fble-profile.h {
      FbleNewProfile
      FbleProfileAddBlock FbleProfileAddBlocks
      FbleFreeProfile
      FbleNewProfileThread FbleForkProfileThread
      FbleFreeProfileThread
      FbleProfileSample FbleProfileRandomSample
      FbleProfileEnterBlock FbleProfileReplaceBlock FbleProfileExitBlock
      FbleProfileReport
    }
    fble-string.h {
      FbleNewString FbleCopyString FbleFreeString
    }
    fble-usage.h {
      FblePrintUsageDoc
    }
    fble-value.h {
      FbleNewValueHeap FbleFreeValueHeap
      FbleRetainValue FbleReleaseValue FbleReleaseValues FbleReleaseValues_
      FbleValueFullGc
      FbleNewStructValue FbleNewStructValue_
      FbleStructValueAccess
      FbleNewUnionValue FbleNewEnumValue
      FbleUnionValueTag FbleUnionValueAccess
      FbleNewListValue FbleNewListValue_ FbleNewLiteralValue
      FbleNewFuncValue FbleNewFuncValue_
      FbleFuncValueInfo FbleEval FbleApply
      FbleNewRefValue FbleAssignRefValue FbleStrictValue
    }
    fble-vector.h {
      FbleVectorInit FbleFreeVector
      FbleVectorExtend FbleVectorAppend
      FbleVectorIncrSize
    }
    fble-version.h {
      FblePrintVersion
    }
  }

  foreach {header funcs} $header_funcs {
    foreach x $funcs {
      fbld_man_dc $::b/include/fble/$x.3 $::s/include/fble/$header $x
      install $::b/include/fble/$x.3 $::config::mandir/man3/$x.3
    }
  }
}
