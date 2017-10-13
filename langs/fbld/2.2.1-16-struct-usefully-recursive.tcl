set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      union Bool(Unit true, Unit false);
      struct NonEmptyBoolList(Bool head, BoolList tail);
      union BoolList(Unit empty, NonEmptyBoolList nonempty);
      func main( ; BoolList);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      # structs can be mutually recursive with unions.
      struct Unit();
      union Bool(Unit true, Unit false);
      struct NonEmptyBoolList(Bool head, BoolList tail);
      union BoolList(Unit empty, NonEmptyBoolList nonempty);

      func main( ; BoolList) {
        BoolList:nonempty(NonEmptyBoolList(Bool:true(Unit()),BoolList:empty(Unit())));
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return BoolList@MainM:nonempty(NonEmptyBoolList@MainM(Bool@MainM:true(Unit@MainM()),BoolList@MainM:empty(Unit@MainM())))
}
