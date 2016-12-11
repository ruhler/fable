
# The return type of a process must match the type of the body.
set prg {
  struct Unit();
  struct Donut();

  proc main( ; ; Unit) {
    $(Donut());
  };
}
expect_malformed $prg main
expect_malformed_b $prg 2

