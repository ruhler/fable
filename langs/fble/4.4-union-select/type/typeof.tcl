fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The type of the union select is the type of the choices.
  Unit@ x = ?(f; true: Unit@(), false: Unit@());
  x;
}
