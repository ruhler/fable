fble-test-compile-error 5:14 {
  @ Unit@ = *();

  # The argument does not compile.
  Unit@ x := zzz;
  !(x);
}
