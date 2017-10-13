set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      func Foo( ; Unit);
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      func Foo( ; Unit) {
        Unit();
      };

      func main( ; Unit) {
        # 'Foo' should refer to a type, but it refers to a function.
        Foo v = Unit();
        Unit();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:11:9
