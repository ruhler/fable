set prg {
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    # One of the conditional's arguments has bad syntax.
    ?(EnumABC:C(Unit());
      EnumXYZ:X(Unit()), ???, EnumXYZ:Z(Unit()));
  };
}
fblc-check-error $prg 9:27
