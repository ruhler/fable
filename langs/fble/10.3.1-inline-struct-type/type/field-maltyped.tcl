fble-test-error 4:15 {
  # The type for the second field of the struct does not compile.
  @ Unit@ = *$();
  *$(Unit@ x, zzz@ x);
}
