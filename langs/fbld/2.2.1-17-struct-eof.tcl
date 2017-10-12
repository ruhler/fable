set prg {
  MainI.fbld {
    mtype MainI {
      type Foo;
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      # A struct declaration should not contain an end of file.
      struct Unit();
      struct Foo(Unit x,
      # } 
  }
}

fbld-check-error $prg MainM MainM.fbld:8:1
