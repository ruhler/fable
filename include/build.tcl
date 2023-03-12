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
  }

  foreach {header funcs} $header_funcs {
    foreach x $funcs {
      man_dc $::b/include/fble/$x.3 $::s/include/fble/$header $x
      install $::b/include/fble/$x.3 $::config::mandir/man3/$x.3
    }
  }
}
