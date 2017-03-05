set prg {
  # Each field of a union declaration needs a name.
  struct Unit();
  union Foo(Unit x, Unit, Unit z);

  func main( ; Foo) {
    Foo:x(Unit());
  };
}
fblc-check-error $prg 4:25
