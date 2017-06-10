set prg {
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    # The variable 'x' is not declared.
    ?(EnumABC:C(Unit()) ;
      EnumXYZ:X(Unit()), x, EnumXYZ:Z(Unit()));
  };
}
fblc-check-error $prg 9:26
