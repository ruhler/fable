set prg {
  Main.mtype {
    mtype Main {
      struct ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType();
      func main( ; ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType();

      func main( ; ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType) {
        ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType();
      };
    };
  }
}

fbld-test $prg "main@Main" {} {
  return ThisIsAStructTypeThatHasAVeryLongNameForTheNameOfTheStructType@Main()
}
