
# There should be no limit on the length of a struct type name.
set prg {
  struct ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType();

  func main( ; ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType) {
    ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType();
  };
}
expect_result ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType() $prg main

