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
        # The condition should take 3 arguments, not 2.
        ?(EnumABC:B(Unit()) ;
          EnumXYZ:X(Unit()), EnumXYZ:Y(Unit()));
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:9:9
