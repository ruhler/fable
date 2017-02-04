set prg {
  # Access a component of a union.
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    ?(EnumABC:C(Unit()) ;
      EnumXYZ:X(Unit()), EnumXYZ:Y(Unit()), EnumXYZ:Z(Unit()));
  };
}

fblc-test $prg main {} "return EnumXYZ:Z(Unit())"
