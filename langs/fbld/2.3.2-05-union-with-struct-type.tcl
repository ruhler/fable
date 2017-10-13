set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct Foo(Unit bar);
      func main( ; Foo);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct Foo(Unit bar);

      func main( ; Foo) {
        # Foo is a struct type, not a union type.
        Foo:bar(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:9
