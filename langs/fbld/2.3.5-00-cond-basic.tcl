set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      union EnumABC(Unit A, Unit B, Unit C);
      union EnumXYZ(Unit X, Unit Y, Unit Z);
      func main( ; EnumXYZ);
    };
  }

  Main.mdefn {
    mdefn Main() {
      # Access a component of a union.
      struct Unit();
      union EnumABC(Unit A, Unit B, Unit C);
      union EnumXYZ(Unit X, Unit Y, Unit Z);

      func main( ; EnumXYZ) {
        ?(EnumABC:C(Unit()) ;
          EnumXYZ:X(Unit()), EnumXYZ:Y(Unit()), EnumXYZ:Z(Unit()));
      };
    };
  }
}

skip fbld-test $prg Main@main {} "return Main@EnumXYZ:Z(Main@Unit())"
