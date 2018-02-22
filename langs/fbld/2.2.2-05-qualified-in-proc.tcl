set prg {
  struct Unit();

  proc foo( ; ; Unit) {
    $(Unit());
  };

  # Qualified references can not be used to access things in a proc.
  struct Broken(Unit@foo uf);
}

fbld-check-error $prg 9:22
