fble-test-error 9:13 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ f = Bool@(false: Unit@());

  # The default argument has the wrong type.
  # TODO: Which branch should the error be on?
  # Seems to depend what order the compiler checks the branches.
  f.?(true: f, : Unit@());
}
