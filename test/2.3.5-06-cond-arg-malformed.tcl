
# An argument of the conditional is not well formed, because the variable
# 'x' is not declared.
set prg {
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    ?(EnumABC:C(Unit()) ;
      EnumXYZ:X(Unit()), x, EnumXYZ:Z(Unit()));
  };
}
expect_malformed $prg main
expect_malformed_b $prg 3
