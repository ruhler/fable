set prg {
  # Each field of a union should be separated by a comma.
  struct Unit();
  union Foo(Unit x, Unit y Unit z);

  func main( ; Foo) {
    Foo:x(Unit());
  };
}
fblc-check-error $prg 4:28
