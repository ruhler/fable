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
      union EnumXYZ(Unit X, Unit Y, Unit Z);

      func main( ; EnumXYZ) {
        # The condition should have 3 arguments, not 0.
        ?(EnumABC:B(Unit()) ; );
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:8:31
