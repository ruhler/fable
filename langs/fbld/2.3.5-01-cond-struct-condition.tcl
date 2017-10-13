set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct StructABC(Unit A, Unit B, Unit C);
      union EnumXYZ(Unit X, Unit Y, Unit Z);
      func main( ; EnumXYZ);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
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

fbld-check-error $prg MainM MainM.fbld:9:11
