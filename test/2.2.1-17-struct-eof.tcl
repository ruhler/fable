
# A struct declaration should not contain an end of file.
set prg {
  struct Unit();
  struct Foo(Unit x, 
}
expect_malformed $prg main
