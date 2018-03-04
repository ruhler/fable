set prg {
  # Qualified references can not be used to access things in an undefined
  # entity.
  struct Broken(Unit@Undefined ui);
}

fbld-check-error $prg 4:22
