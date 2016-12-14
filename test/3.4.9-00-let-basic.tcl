
# Test a basic let expression.
set prg {
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; A) {
    Unit v = Unit();
    A(v, v);
  };
}
expect_result A(Unit(),Unit()) $prg main
# skip: requires support for let expression
skip expect_result_b "" $prg 2

