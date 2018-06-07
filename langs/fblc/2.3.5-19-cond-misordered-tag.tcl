set prg {
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    # The conditional arguments are not in the right order.
    ?(EnumABC:C(Unit());
      B: EnumXYZ:Y(Unit()), A: EnumXYZ:X(Unit()), C: EnumXYZ:Z(Unit()));
  };
}
fblc-check-error $prg 9:7
