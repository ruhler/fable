
#include <fble/fble-value.h>       // for FbleValue, etc.

/**
 * @func[Not] FbleRunFunction for 'Not' foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is @l{(Bool@) { Bool@; }}.
 *
 *  @sideeffects None
 */
static FbleValue* Not(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  FbleValue* arg = args[0];
  switch (FbleUnionValueTag(arg, 1)) {
    case 0: return FbleNewEnumValue(heap, 1, 1);
    case 1: return FbleNewEnumValue(heap, 1, 0);
  }
  return NULL;
}

FbleForeign _Fble_2f_SpecTests_2f_10_2e_1_2d_ForeignValue_2f_Basic_2f_Basic_25__2e_Not = {
  .path = "/SpecTests/'10.1-ForeignValue'/Basic/Basic%",
  .name = "Not",
  .num_args = 1,
  .max_call_args = 0,
  .run = &Not
};

FbleForeign _Fble_2f_SpecTests_2f_10_2e_1_2d_ForeignValue_2f_Basic_2f_FunnyName_25__2e_N_21__2e__2f_t = {
  .path = "/SpecTests/'10.1-ForeignValue'/Basic/FunnyName%",
  .name = "N!./t",
  .num_args = 1,
  .max_call_args = 0,
  .run = &Not
};

FbleForeign _Fble_2f_SpecTests_2f_10_2e_1_2d_ForeignValue_2f_Basic_2f_Unused_25__2e_Not = {
  .path = "/SpecTests/'10.1-ForeignValue'/Basic/Unused%",
  .name = "Not",
  .num_args = 1,
  .max_call_args = 0,
  .run = &Not
};

/**
 * @func[Poly] FbleRunFunction for 'Nothing' foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is @l{(Bool@) { Bool@; }}.
 *
 *  @sideeffects None
 */
static FbleValue* Poly(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  return FbleNewUnionValue(heap, 1, 0, args[0]);
}

FbleForeign _Fble_2f_SpecTests_2f_10_2e_1_2d_ForeignValue_2f_Basic_2f_Poly_25__2e_Nothing = {
  .path = "/SpecTests/'10.1-ForeignValue'/Basic/Poly%",
  .name = "Nothing",
  .num_args = 1,
  .max_call_args = 0,
  .run = &Poly
};

/**
 * @func[True] FbleRunFunction for True foreign value.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is @l{Bool@}.
 *
 *  @sideeffects None
 */
static FbleValue* True(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  return FbleNewEnumValue(heap, 1, 0);
}

FbleForeign _Fble_2f_SpecTests_2f_10_2e_1_2d_ForeignValue_2f_Basic_2f_NonFunc_25__2e_True = {
  .path = "/SpecTests/'10.1-ForeignValue'/Basic/NonFunc%",
  .name = "True",
  .num_args = 0,
  .max_call_args = 0,
  .run = &True
};

/**
 * @func[False] FbleRunFunction for 'False' foreign value.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is @l{Bool@}.
 *
 *  @sideeffects None
 */
static FbleValue* False(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  return FbleNewEnumValue(heap, 1, 1);
}

FbleForeign _Fble_2f_SpecTests_2f_10_2e_1_2d_ForeignValue_2f_Basic_2f_NonFunc_25__2e_False = {
  .path = "/SpecTests/'10.1-ForeignValue'/Basic/NonFunc%",
  .name = "False",
  .num_args = 0,
  .max_call_args = 0,
  .run = &False
};

// Foreign value with same name but different module as another foreign
// value, to test we link with the right one.
FbleForeign _Fble_2f_SpecTests_2f_10_2e_1_2d_ForeignValue_2f_Basic_2f_ModuleMatters_25__2e_False = {
  .path = "/SpecTests/'10.1-ForeignValue'/Basic/ModuleMatters%",
  .name = "False",
  .num_args = 0,
  .max_call_args = 0,
  .run = &True
};

/**
 * @func[FbleTestRegisterForeignValues] Register foreign values for test purposes.
 *  @arg[FbleValueHeap*][heap] The heap to register with.
 *  @sideeffects Registers all the test ffi functions.
 */
void FbleTestRegisterForeignValues(FbleValueHeap* heap) {
  FbleRegisterForeignValue(heap, &_Fble_2f_SpecTests_2f_10_2e_1_2d_ForeignValue_2f_Basic_2f_Basic_25__2e_Not);
  FbleRegisterForeignValue(heap, &_Fble_2f_SpecTests_2f_10_2e_1_2d_ForeignValue_2f_Basic_2f_FunnyName_25__2e_N_21__2e__2f_t);
  FbleRegisterForeignValue(heap, &_Fble_2f_SpecTests_2f_10_2e_1_2d_ForeignValue_2f_Basic_2f_Unused_25__2e_Not);
  FbleRegisterForeignValue(heap, &_Fble_2f_SpecTests_2f_10_2e_1_2d_ForeignValue_2f_Basic_2f_Poly_25__2e_Nothing);
  FbleRegisterForeignValue(heap, &_Fble_2f_SpecTests_2f_10_2e_1_2d_ForeignValue_2f_Basic_2f_NonFunc_25__2e_True);
  FbleRegisterForeignValue(heap, &_Fble_2f_SpecTests_2f_10_2e_1_2d_ForeignValue_2f_Basic_2f_NonFunc_25__2e_False);
  FbleRegisterForeignValue(heap, &_Fble_2f_SpecTests_2f_10_2e_1_2d_ForeignValue_2f_Basic_2f_ModuleMatters_25__2e_False);
}
