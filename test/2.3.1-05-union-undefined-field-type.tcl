
# A field can have any type that is defined somewhere in the program.
# Which means it cannot have a type that is not defined.
set prg {
  struct Unit();
  union A(Unit x, Donut y);

  func main( ; A) {
    A:x(Unit());
  };
}
expect_malformed $prg main

