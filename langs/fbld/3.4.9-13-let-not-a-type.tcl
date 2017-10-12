set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      func Foo( ; Unit);
      func main( ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
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
