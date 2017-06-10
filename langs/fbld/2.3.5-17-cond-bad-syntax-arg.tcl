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
      struct Unit();
      union EnumABC(Unit A, Unit B, Unit C);
      union EnumXYZ(Unit X, Unit Y, Unit Z);

      func main( ; EnumXYZ) {
        # One of the conditional's arguments has bad syntax.
        ?(EnumABC:C(Unit());
        EnumXYZ:X(Unit()), ???, EnumXYZ:Z(Unit()));
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:10:29
