set prg {
  # The condition should have 3 arguments, not 0.
  struct Unit();
  union EnumABC(Unit A, Unit B, Unit C);
  union EnumXYZ(Unit X, Unit Y, Unit Z);

  func main( ; EnumXYZ) {
    ?(EnumABC:B(Unit()) ; );
  };
}
# TODO: Where exactly should the error be?
fblc-check-error $prg 8:5
