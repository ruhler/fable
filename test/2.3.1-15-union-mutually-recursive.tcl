set prg {
  # unions can be mutually recursive.
  struct Unit();
  union Foo(Unit x, Bar y);
  union Bar(Unit x, Foo y);

  func main( ; Foo) {
    Foo:y(Bar:y(Foo:x(Unit()))); 
  };
}

fblc-test $prg main {} "return Foo:y(Bar:y(Foo:x(Unit())))"
