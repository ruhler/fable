fble-test-error 11:5 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Maybe@ = +(Bool@ just, Unit@ nothing);
  Maybe@ x = Maybe@(just: true);

  # The field name is missing.
  x.;
}
