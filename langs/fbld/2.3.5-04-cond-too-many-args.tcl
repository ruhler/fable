set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      union EnumABC(Unit A, Unit B, Unit C);
      union EnumXYZ(Unit X, Unit Y, Unit Z);
      func main( ; EnumXYZ);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      union EnumABC(Unit A, Unit B, Unit C);
      union EnumXYZ(Unit X, Unit Y, Unit Z);

      func main( ; EnumXYZ) {
        # The condition should take 3 args, not 4.
        ?(EnumABC:B(Unit()) ;
          EnumXYZ:X(Unit()), EnumXYZ:Y(Unit()), EnumXYZ:Z(Unit()), EnumXYZ:X(Unit()));
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:9:9
