set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      union EnumABC(Unit A, Unit B, Unit C);
      union EnumXYZ(Unit X, Unit Y, Unit Z);
      func main( ; EnumXYZ);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
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

fbld-check-error $prg MainM MainM.fbld:10:29
