set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      struct A(Unit x, Unit y);
      union Foo(Unit bar, A sludge);

      func main( ; Foo) {
        # No field is provided for the constructor.
        Foo:(Unit());
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:9:13
