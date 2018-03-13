set prg {
  interf I {
    struct Unit();
    struct Coord(Unit x, Unit y);
  };

  module M(I) {
    struct Unit();

    # The argument name 'z' here doesn't match the declaration in interf I.
    struct Coord(Unit z, Unit y);
  };
}

fbld-check-error $prg 11:23
