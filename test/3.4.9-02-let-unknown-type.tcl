
# The type of variables defined in let statements must be declared.

set prg {
  struct Unit();

  func main( ; Unit) {
    // The type 'Foo' has not been declared.
    Foo v = Unit();
    Unit();
  };
}

fblc-check-error $prg

