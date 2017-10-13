set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      union Foo(Unit x, Bar y);   # mutually recursive with Bar
      union Bar(Unit x, Foo y);   # mutually recursive with Foo
      func main( ; Foo);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      union Foo(Unit x, Bar y);
      union Bar(Unit x, Foo y);

      func main( ; Foo) {
        Foo:y(Bar:y(Foo:x(Unit()))); 
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Foo@MainM:y(Bar@MainM:y(Foo@MainM:x(Unit@MainM())))
}
