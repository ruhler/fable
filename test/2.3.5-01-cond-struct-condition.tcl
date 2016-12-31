set prg {
  // The condition of the conditional should have union type, not struct type.
  struct Unit();
  struct StructABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    ?(StructABC(Unit(), Unit(), Unit()) ;
      EnumXYZ:X(Unit()), EnumXYZ:Y(Unit()), EnumXYZ:Z(Unit()));
  };
}
fblc-check-error $prg 8:7
