
# The conditional is missing one of its arguments.
set prg {
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    ?(EnumABC:C(Unit());
      EnumXYZ:X(Unit()), , EnumXYZ:Z(Unit()));
  };
}
expect_malformed $prg main
expect_malformed_b $prg 3
