set prg {
  interf I {
    struct Unit();
  };

  module M(I) {
    # Unit was public in the interface, it should be public here too.
    abst struct Unit();
  };
}

fbld-check-error $prg 8:17
