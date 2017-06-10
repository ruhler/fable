set prg {
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    # Not all arguments to the conditional have the same type.
    ?(EnumABC:C(Unit()) ;
      EnumXYZ:X(Unit()), EnumABC:B(Unit()), EnumXYZ:Z(Unit()));
  };
}
# TODO: Where should the error be?
fblc-check-error $prg 9:26
