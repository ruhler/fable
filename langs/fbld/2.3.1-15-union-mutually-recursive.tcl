set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      union Foo(Unit x, Bar y);   # mutually recursive with Bar
      union Bar(Unit x, Foo y);   # mutually recursive with Foo
      func main( ; Foo);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      union Foo(Unit x, Bar y);
      union Bar(Unit x, Foo y);

      func main( ; Foo) {
        Foo:y(Bar:y(Foo:x(Unit()))); 
      };
    };
  }
}

fbld-test $prg "main@Main" {} {
  return Foo@Main:y(Bar@Main:y(Foo@Main:x(Unit@Main())))
}
