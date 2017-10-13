set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      proc main( Unit+ out ; ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      proc main( Unit+ out ; ; Unit) {
        +out(Unit());
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  get out Unit@MainM()
  return Unit@MainM()
}
