fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Maybe@ = +(Bool@ just, Unit@ nothing);
  Maybe@ x = Maybe@(just: true);

  # The type of an access expression is the type of the field being accessed.
  Bool@ result = x.just;
  result;
}
