# Test a simple process.
set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      proc main( ; ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      proc main( ; ; Unit) {
        $(Unit());
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Unit@MainM()
}
