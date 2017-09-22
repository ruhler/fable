set prg {
  Main.mtype {
    mtype Main<> {
      type Foo;
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      # A struct declaration should not contain an end of file.
      struct Unit();
      struct Foo(Unit x,
      # } 
  }
}

fbld-check-error $prg Main Main.mdefn:8:1
