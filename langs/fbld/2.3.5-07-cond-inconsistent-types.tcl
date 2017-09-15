set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      union EnumABC(Unit A, Unit B, Unit C);
      union EnumXYZ(Unit X, Unit Y, Unit Z);
      func main( ; EnumXYZ);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      union EnumABC(Unit A, Unit B, Unit C);
      union EnumXYZ(Unit X, Unit Y, Unit Z);

      func main( ; EnumXYZ) {
        # Not all arguments to the conditional have the same type.
        ?(EnumABC:C(Unit()) ;
          EnumXYZ:X(Unit()), EnumABC:B(Unit()), EnumXYZ:Z(Unit()));
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:10:30
