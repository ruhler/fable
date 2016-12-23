
# The condition should take 3 arguments, not 2.
set prg {
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    ?(EnumABC:B(Unit()) ;
      EnumXYZ:X(Unit()), EnumXYZ:Y(Unit()));
  };
}
fblc-check-error $prg
