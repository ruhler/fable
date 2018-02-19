set prg {
  struct Unit();

  module A {
    module B {
      # You can't import something from a grandparent without first importing
      # it into the parent module.
      import @ { Unit; };

      union Fruit(Unit apple, Unit banana);
    };
  };

  func main( ; Unit) {
    Unit();
  };
}

fbld-check-error $prg 8:18
