
# The conditional is missing its first argument.
set prg {
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    ?(EnumABC:C(Unit());
      , EnumXYZ:Y(Unit()), EnumXYZ:Z(Unit()));
  };
}
fblc-check-error $prg
