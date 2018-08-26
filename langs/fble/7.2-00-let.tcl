fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # Basic use of let expression
  Bool@ x = Bool@(false: Unit@());
  x;
}
