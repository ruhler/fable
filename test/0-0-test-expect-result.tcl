
# Test the most basic 'expect_result' test.

set prg {
  struct Unit();

  func main( ; Unit) {
    Unit();
  };
}

expect_result Unit() $prg main

# Test in binary format.
# Program(NonEmptyDeclList(
#   Decl:00-struct(TypeList:0-nul()), DeclList:1-cons(NonEmptyDeclList(
#   Decl:10-func(
#     TypeList:0-nul(),
#     DeclId:00-0(),
#     Expr:001-apply(AppExpr(
#       DeclId:00-0()),
#       ExprList:0-nul()))
#   ), DeclList:0-nil()))
set bits 0001100000010000
expect_result_b "" $bits 1

