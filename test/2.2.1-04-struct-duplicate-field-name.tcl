
# Each field of a struct type must have a different name.
set prg {
  struct Unit();
  struct Donut();
  struct BadStruct(Unit x, Donut x);

  func main( ; BadStruct) {
    BadStruct(Unit(), Donut());
  };
}
expect_malformed $prg main
expect_malformed_b $prg 3


# Even if the types are the same.
set prg {
  struct Unit();
  struct BadStruct(Unit x, Unit x);

  func main( ; BadStruct) {
    BadStruct(Unit(), Unit());
  };
}
expect_malformed $prg main
expect_malformed_b $prg 2
