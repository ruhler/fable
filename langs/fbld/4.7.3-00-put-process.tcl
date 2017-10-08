set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      proc main( Unit+ out ; ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      proc main( Unit+ out ; ; Unit) {
        +out(Unit());
      };
    };
  }
}

fbld-test $prg "main@Main" {} {
  get out Unit@Main()
  return Unit@Main()
}
