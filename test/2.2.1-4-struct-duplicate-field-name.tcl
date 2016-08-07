
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

