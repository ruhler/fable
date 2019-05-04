fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Maybe@ = +(Bool@ just, Unit@ nothing);

  # Test the type of a union value expression.
  Maybe@ x = Maybe@(just: true);
  x;
}
