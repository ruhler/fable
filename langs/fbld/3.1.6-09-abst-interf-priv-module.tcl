set prg {
  interf I {
    type Unit;
  };

  module M(I) {
    # Unit was abstract in the interface, it should be abstract here too.
    priv struct Unit();
  };
}

fbld-check-error $prg 8:17
