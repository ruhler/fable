
# All arguments to the conditional must have the same type.
set prg {
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    ?(EnumABC:C(Unit()) ;
      EnumXYZ:X(Unit()), EnumABC:B(Unit()), EnumXYZ:Z(Unit()));
  };
}
expect_malformed $prg main
expect_malformed_b $prg 3
