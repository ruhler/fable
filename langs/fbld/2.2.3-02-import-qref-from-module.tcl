set prg {
  # We can import a qref from a module.
  import M { Unit@A; };

  module M {
    module A {
      struct Unit();
    };
  };

  func main( ; Unit) {
    Unit();
  };
}

fbld-test $prg "main" {} {
  return Unit@A@M()
}
