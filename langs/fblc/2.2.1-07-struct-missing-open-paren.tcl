
# A struct declaration must have an open paren.
set prg {
  struct Unit();
  struct Foo Unit x, Unit y);
}
fblc-check-error $prg 3:14
