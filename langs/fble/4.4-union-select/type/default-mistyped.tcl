fble-test-error 7:18 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ f = Bool@(false: Unit@());

  # The default argument has the wrong type.
  f.?(true: f, : Unit@());
}
