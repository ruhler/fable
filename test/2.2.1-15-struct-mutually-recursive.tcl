
# structs can be mutually recursive.
set prg {
  struct Unit();
  struct Foo(Bar x);
  struct Bar(Foo y);

  func main( ; Unit) {
    Unit();
  };
}
expect_result Unit() $prg main
expect_result_b 0 $prg 3

