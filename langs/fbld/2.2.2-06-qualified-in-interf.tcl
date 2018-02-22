set prg {
  interf I {
    struct Unit();
  };

  module M (I) {
    struct Unit();
  };

  # Qualified references can not be used to access things in an interface.
  struct Broken(Unit@I ui);
}

fbld-check-error $prg 11:22
