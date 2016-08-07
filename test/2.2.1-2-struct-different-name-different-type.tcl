
# Two structs with different names are considered different types, even if
# they have the same fields.

set prg {
  struct Unit();
  struct Donut();

  func main( ; Unit) {
    Donut();
  };
}

expect_malformed $prg main

