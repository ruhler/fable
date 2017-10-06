set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      func Foo( ; Unit);
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
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

fbld-check-error $prg Main Main.mdefn:11:9
