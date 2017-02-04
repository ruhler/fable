
# structs can be mutually recursive with unions.
set prg {
  struct Unit();
  union Bool(Unit true, Unit false);
  struct NonEmptyBoolList(Bool head, BoolList tail);
  union BoolList(Unit empty, NonEmptyBoolList nonempty);

  func main( ; BoolList) {
    BoolList:nonempty(NonEmptyBoolList(Bool:true(Unit()),BoolList:empty(Unit())));
  };
}
expect_result BoolList:nonempty(NonEmptyBoolList(Bool:true(Unit()),BoolList:empty(Unit()))) $prg main
