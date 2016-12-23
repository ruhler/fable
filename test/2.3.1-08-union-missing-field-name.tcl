
# Each field of a union declaration needs a name.
set prg {
  struct Unit();
  union Foo(Unit x, Unit, Unit z);

  func main( ; Foo) {
    Foo:x(Unit());
  };
}
fblc-check-error $prg

