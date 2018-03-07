set prg {
  struct Unit();

  func Foo( ; Unit) {
    Unit();
  };

  # Can't have a static parameter that shadows an existing entity.
  struct Pair<type Foo>(Foo first, Foo second);
}

fbld-check-error $prg 9:20
