set prg {
  module A {
    struct SA();
    union TA(SA sa);

    func mkTA( ; TA) {
      TA:sa(SA());
    };
  };

  # The body of this function depends on entities imported from both module A
  # and B. The compiler should not have any problems with the fact that it is
  # declared between them instead of after them.
  func tb(; TB@B) {
    ?(mkTA@A() ; mkTB@B());
  };

  module B {
    struct TB();

    func mkTB( ; TB) {
      TB();
    };
  };

  func main( ; TB@B) {
    tb();
  };
}

fbld-test $prg "main" {} {
  return TB@B()
}
