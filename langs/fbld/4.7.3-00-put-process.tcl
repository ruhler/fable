set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      proc main( Unit+ out ; ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
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
