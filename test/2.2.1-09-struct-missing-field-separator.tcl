
# Each field of a struct should be separated by a comma.
set prg {
  struct Unit();
  struct Foo(Unit x, Unit y Unit z);

  func main( ; Foo) {
    Foo(Unit(), Unit(), Unit());
  };
}
fblc-check-error $prg

