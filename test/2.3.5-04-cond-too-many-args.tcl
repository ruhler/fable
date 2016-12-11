
# The condition should take 3 args, not 4.
set prg {
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    ?(EnumABC:B(Unit()) ;
      EnumXYZ:X(Unit()), EnumXYZ:Y(Unit()), EnumXYZ:Z(Unit()), EnumXYZ:X(Unit()));
  };
}
expect_malformed $prg main
expect_malformed_b $prg 3
