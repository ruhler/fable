set prg {
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    # One of the conditional argument's tags is bad.
    ?(EnumABC:C(Unit());
      A: EnumXYZ:X(Unit()), BOGUS: EnumXYZ:Y(Unit()), C: EnumXYZ:Z(Unit()));
  };
}
fblc-check-error $prg 9:29
