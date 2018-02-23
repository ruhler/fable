set prg {
  interf I {
    type Unit;
  };

  module M(I) {
    # Unit was abstract in the interface, it should be defined here.
  };
}

fbld-check-error $prg 6:10
