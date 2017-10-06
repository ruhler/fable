set prg {
  Main.mtype {
    mtype Main {
      struct Unit();

      # structs can be mutually recursive.
      struct Foo(Bar x);
      struct Bar(Foo y);

      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      struct Foo(Bar x);
      struct Bar(Foo y);

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-test $prg "main@Main" {} {
  return Unit@Main()
}
