set prg {

  module A {
    module B {
      module C {
        struct D();
      };
    };
  };

  # There should be no problem importing D@C@B@A, even if the import
  # statements are in a slightly unnatural order.
  import A { B; };
  import C { D; };
  import B { C; };

  func main( ; D) {
    D();
  };
}

fbld-test $prg "main" {} {
  return D@C@B@A()
}

