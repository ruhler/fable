set prg {
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    # The condition should take 3 arguments, not 2.
    ?(EnumABC:B(Unit()) ;
      EnumXYZ:X(Unit()), EnumXYZ:Y(Unit()));
  };
}
# TODO: Where exactly should the error be?
fblc-check-error $prg 8:5
