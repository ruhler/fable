fble-test-error 6:15 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # The body does not compile.
  (Bool@ x) { zzz; };
}

