fble-test-compile-error 4:14 {
  # The type for the second field of the struct does not compile.
  @ Unit@ = *();
  *(Unit@ x, zzz@ x);
}
