fble-test {
  @ Unit@ = *$();
  @ Bool@ = +$(Unit@ true, Unit@ false);

  # Basic use of inline let expression.
  Bool@ x $= Bool@(false: Unit@());
  $(x).false;
}
