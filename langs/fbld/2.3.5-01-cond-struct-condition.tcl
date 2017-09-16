set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      struct StructABC(Unit A, Unit B, Unit C);
      union EnumXYZ(Unit X, Unit Y, Unit Z);
      func main( ; EnumXYZ);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      struct StructABC(Unit A, Unit B, Unit C);
      union EnumXYZ(Unit X, Unit Y, Unit Z);

      func main( ; EnumXYZ) {
        # The condition of the conditional should have union type, not struct type.
        ?(StructABC(Unit(), Unit(), Unit()) ;
          EnumXYZ:X(Unit()), EnumXYZ:Y(Unit()), EnumXYZ:Z(Unit()));
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:9:11
