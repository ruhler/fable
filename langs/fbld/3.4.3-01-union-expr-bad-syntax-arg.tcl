set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct A(Unit x, Unit y);
      union Foo(Unit bar, A sludge);
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
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
fbld-check-error $prg MainM MainM.fbld:9:18
