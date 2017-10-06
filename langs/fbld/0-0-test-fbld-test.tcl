# Test the most basic 'fbld-test' test.
set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-test $prg "main@Main" {} {
  return Unit@Main()
}
