set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      struct A(Unit x, Unit y);
      union Foo(Unit bar, A sludge);
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      struct A(Unit x, Unit y);
      union Foo(Unit bar, A sludge);

      func main( ; Unit) {
        # The argument isn't valid syntax.
        Foo:bar(???).bar;
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:9:18
