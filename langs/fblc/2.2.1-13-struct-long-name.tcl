set prg {
  # There should be no limit on the length of a struct type name.
  struct ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType();

  func main( ; ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType) {
    ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType();
  };
}

fblc-test $prg main {} "return ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType()"
