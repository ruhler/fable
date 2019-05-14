fble-test-error 8:13 {
  @ Unit@ = *();

  # Bool@ is treated as an abstract type in the bindings of the let where it
  # is declared, which means you are not allowed to, for example, allocate a
  # value of it in the bindng.
  @ Bool@ = +(Unit@ true, Unit@ false),
  Unit@ u = Bool@(true: Unit@()).true;
  u;
}
