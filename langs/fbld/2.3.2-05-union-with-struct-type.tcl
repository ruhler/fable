set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      struct Foo(Unit bar);
      func main( ; Foo);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      struct Foo(Unit bar);

      func main( ; Foo) {
        # Foo is a struct type, not a union type.
        Foo:bar(Unit());
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:8:9
