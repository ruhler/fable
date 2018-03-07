set prg {
  struct Unit();

  func f<struct S(Unit a, Unit b)>( ; Unit) {
    S(Unit(), Unit()).b;
  };

  struct Foo(Unit a, Unit x);

  func main( ; Unit) {
    # The second field of Foo has the wrong name.
    f<Foo>();
  };
}

fbld-check-error $prg 12:7
