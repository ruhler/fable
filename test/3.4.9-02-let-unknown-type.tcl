
# The type of variables defined in let statements must be declared.

set prg {
  struct Unit();

  func main( ; Unit) {
    // The type 'Foo' has not been declared.
    Foo v = Unit();
    Unit();
  };
}

expect_malformed $prg main
expect_malformed_b $prg 1

