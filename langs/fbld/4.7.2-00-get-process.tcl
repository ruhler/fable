set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      proc main( Unit <~ in ; ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();

      proc main( Unit <~ in ; ; Unit) {
        ~in();
      };
    };
  }
}

fbld-test $prg "main@Main" {} {
  put in Unit@Main()
  return Unit@Main()
}
