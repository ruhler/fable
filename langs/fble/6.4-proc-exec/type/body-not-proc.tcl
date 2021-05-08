fble-test-compile-error 8:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # The body of the exec is not a process.
  Bool@ x := !(true);
  x;
}
