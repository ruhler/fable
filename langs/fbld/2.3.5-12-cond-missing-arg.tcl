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
        # The conditional is missing one of its arguments.
        ?(EnumABC:C(Unit());
          EnumXYZ:X(Unit()), , EnumXYZ:Z(Unit()));
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:10:30
