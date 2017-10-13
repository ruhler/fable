set prg {
  MainI.fbld {
    interf MainI {
      struct ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType();
      func main( ; ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType();

      func main( ; ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType) {
        ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType();
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType@MainM()
}
