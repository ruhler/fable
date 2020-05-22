fble-test-error 7:14 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # The argument to an exec is not a process.
  Bool@ x := true;
  !(x);
}
