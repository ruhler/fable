fble-test-error 6:13 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # The binding expression does not compile.
  Bool@ x = zzz;
  x;
}
