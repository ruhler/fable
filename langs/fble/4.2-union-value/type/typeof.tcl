fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  @ Maybe@ = +(Bool@ just, Unit@ nothing);

  # Test the type of a union value expression.
  Maybe@ x = Maybe@(just: true);
  x;
}
