set prg {
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    # The condition should take 3 args, not 4.
    ?(EnumABC:B(Unit()) ;
      A: EnumXYZ:X(Unit()), B: EnumXYZ:Y(Unit()), C: EnumXYZ:Z(Unit()), D: EnumXYZ:X(Unit()));
  };
}
fblc-check-error $prg 8:5
