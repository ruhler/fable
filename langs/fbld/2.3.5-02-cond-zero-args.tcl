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
      union EnumXYZ(Unit X, Unit Y, Unit Z);

      func main( ; EnumXYZ) {
        # The condition should have 3 arguments, not 0.
        ?(EnumABC:B(Unit()) ; );
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:31
