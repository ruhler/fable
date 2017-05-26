set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      union Bool(Unit true, Unit false);
      struct NonEmptyBoolList(Bool head, BoolList tail);
      union BoolList(Unit empty, NonEmptyBoolList nonempty);
      func main( ; BoolList);
    };
  }

  Main.mdefn {
    mdefn Main() {
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

fbld-test $prg main@Main {} "return BoolList@Main:nonempty(NonEmptyBoolList@Main(Bool@Main:true(Unit@Main()),BoolList@Main:empty(Unit@Main())))"
