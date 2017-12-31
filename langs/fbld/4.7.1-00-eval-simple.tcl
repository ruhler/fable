set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      proc main( ; ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
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
