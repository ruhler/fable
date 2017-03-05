
# A struct declaration should not contain an end of file.
set prg {
  struct Unit();
  struct Foo(Unit x, 
}
fblc-check-error $prg 5:1
