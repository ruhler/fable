set prg {
  struct Unit();

  func foo( ; Unit) {
    Unit();
  };

  # Qualified references can not be used to access things in a func.
  struct Broken(Unit@foo uf);
}

fbld-check-error $prg 9:22
