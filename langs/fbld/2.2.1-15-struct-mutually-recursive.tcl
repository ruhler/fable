set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();

      # structs can be mutually recursive.
      struct Foo(Bar x);
      struct Bar(Foo y);

      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct Foo(Bar x);
      struct Bar(Foo y);

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Unit@MainM()
}
