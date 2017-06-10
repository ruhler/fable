set prg {
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    # The conditional has a comma instead of a semicolon after the condition.
    ?(EnumABC:C(Unit()),
      EnumXYZ:X(Unit()), EnumXYZ:Y(Unit()), EnumXYZ:Z(Unit()));
  };
}
fblc-check-error $prg 8:24
