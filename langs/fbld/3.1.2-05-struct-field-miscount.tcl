set prg {
  struct Unit();

  func f<struct S(Unit a, Unit b)>( ; Unit) {
    S(Unit(), Unit()).b;
  };

  struct Foo(Unit a, Unit b, Unit c);

  func main( ; Unit) {
    # Foo has too many fields
    f<Foo>();
  };
}

fbld-check-error $prg 12:7
