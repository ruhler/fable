set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      proc main( Unit- in ; ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
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
