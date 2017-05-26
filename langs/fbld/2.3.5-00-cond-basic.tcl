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

fbld-test $prg main@Main {} "return EnumXYZ@Main:Z(Unit@Main())"
