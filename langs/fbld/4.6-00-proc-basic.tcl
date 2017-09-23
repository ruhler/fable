# Test a simple process.
set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      proc main( ; ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();

      proc main( ; ; Unit) {
        $(Unit());
      };
    };
  }
}

fbld-test $prg "main@Main<;>" {} {
  return Unit@Main<;>()
}
