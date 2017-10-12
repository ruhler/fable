set prg {
  MainI.fbld {
    mtype MainI {
      struct ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType();
      func main( ; ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
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
