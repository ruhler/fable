set prg {
  interf UnitI {
    struct Unit();
  };

  module A {
    import @ { UnitI; };
    module subA(UnitI) {
      struct Unit();
    };
  };

  # The proto of this function depends on entities imported from both module A
  # and B. The compiler should not have any problems with the fact that it is
  # declared between them instead of after them.
  func tb(Unit@subB@B b; Unit@subA@A) {
    Unit@subA@A();
  };

  module B {
    import @ { UnitI; };
    module subB(UnitI) {
      struct Unit();
    };
  };

  func main( ; Unit@subA@A) {
    tb(Unit@subB@B());
  };
}

fbld-test $prg "main" {} {
  return Unit@subA@A()
}
