fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # The body of the function is not evaluated before the function is applied.
  (Bool@ x) { true.false; };
}

