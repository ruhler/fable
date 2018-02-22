set prg {
  module A {
    module B {
      struct Unit();
    };
  };

  # Verify we can access Unit using nested qualified references.
  func main( ; Unit@B@A) {
    Unit@B@A();
  };
}

fbld-test $prg "main" {} {
  return Unit@B@A()
}
