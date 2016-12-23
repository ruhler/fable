
# The condition should have 3 arguments, not 0.
set prg {
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    ?(EnumABC:B(Unit()) ; );
  };
}
fblc-check-error $prg
