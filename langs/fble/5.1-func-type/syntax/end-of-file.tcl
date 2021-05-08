fble-test-compile-error 8:1 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # The file ends before the function type is completed.
  (Bool@,
}
