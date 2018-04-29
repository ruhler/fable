set prg {
  struct Unit();
  struct A(Unit B);

  # A refers to a struct, not a module. You can't import from it.
  import A { B; };
}

fbld-check-error $prg 6:10
