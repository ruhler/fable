set prg {
  # Access a component of a union.
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    ?(EnumABC:C(Unit()) ;
      A: EnumXYZ:X(Unit()), B: EnumXYZ:Y(Unit()), C: EnumXYZ:Z(Unit()));
  };
}

fblc-test $prg main {} "return EnumXYZ:Z(Unit())"
