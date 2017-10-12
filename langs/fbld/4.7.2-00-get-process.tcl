set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      proc main( Unit- in ; ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      proc main( Unit- in ; ; Unit) {
        -in();
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  put in Unit@MainM()
  return Unit@MainM()
}
