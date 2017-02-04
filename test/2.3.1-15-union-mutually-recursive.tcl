
# unions can be mutually recursive.
set prg {
  struct Unit();
  union Foo(Unit x, Bar y);
  union Bar(Unit x, Foo y);

  func main( ; Foo) {
    Foo:y(Bar:y(Foo:x(Unit()))); 
  };
}
expect_result Foo:y(Bar:y(Foo:x(Unit()))) $prg main
