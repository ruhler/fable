set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      struct A(Unit x, Unit y);
      union Foo(Unit bar, A sludge);
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main() {
      struct Unit();
      struct A(Unit x, Unit y);
      union Foo(Unit bar, A sludge);

      func main( ; Unit) {
        # The variable 'x' has not  been declared.
        Foo:bar(x).bar;
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:9:17
