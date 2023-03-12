namespace eval "include" {
  dist_s $::s/include/build.tcl
  foreach {x} [build_glob $::s/include/fble *.h] {
    dist_s $x
    install $x $::config::includedir/fble/[file tail $x]
  }

  set header_funcs {
    fble-alloc.h {
      FbleRawAlloc FbleAlloc FbleAllocExtra FbleArrayAlloc
      FbleFree
      FbleNewStackAllocator FbleFreeStackAllocator
      FbleRawStackAlloc FbleStackAlloc FbleStackAllocExtra
      FbleStackFree
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
      FbleThreadTailCall FbleThreadTailCall_
      FbleThreadSample
      FbleThreadEnterBlock FbleThreadReplaceBlock FbleThreadExitBlock
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
      FbleProfileSample
      FbleProfileEnterBlock FbleProfileReplaceBlock FbleProfileExitBlock
      FbleProfileReport
    }
    fble-string.h {
      FbleNewString FbleCopyString FbleFreeString
    }
    fble-value.h {
      FbleNewValueHeap FbleFreeValueHeap
      FbleRetainValue FbleReleaseValue FbleReleaseValues FbleReleaseValues_
      FbleValueAddRef
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
      man_dc $::b/include/fble/$x.3 $::s/include/fble/$header $x
      install $::b/include/fble/$x.3 $::config::mandir/man3/$x.3
    }
  }
}
