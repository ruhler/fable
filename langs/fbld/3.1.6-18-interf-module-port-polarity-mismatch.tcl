set prg {
  interf I {
    struct Unit();
    proc p(Unit+ out; ; Unit);
  };

  module M(I) {
    struct Unit();

    # The polarity of the port should be +, not -.
    proc p(Unit- out; ; Unit) {
      $(Unit());
    };
  };
}

fbld-check-error $prg 11:18
