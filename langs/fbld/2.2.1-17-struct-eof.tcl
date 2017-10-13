set prg {
  MainI.fbld {
    interf MainI {
      type Foo;
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      # A struct declaration should not contain an end of file.
      struct Unit();
      struct Foo(Unit x,
      # } 
  }
}

fbld-check-error $prg MainM MainM.fbld:8:1
