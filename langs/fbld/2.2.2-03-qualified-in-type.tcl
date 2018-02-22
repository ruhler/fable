set prg {
  struct Unit();
  struct Pair(Unit a, Unit b);

  # Qualified references can not be used to access things in a type.
  struct Broken(Unit@Pair up);
}

fbld-check-error $prg 6:22
