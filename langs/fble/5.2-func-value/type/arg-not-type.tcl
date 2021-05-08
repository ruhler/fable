fble-test-compile-error 8:4 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ Maybe@ = +(Bool@ just, Unit@ nothing);

  # The argument type is not a type.
  Unit@ u = Unit@();
  (u x) { Maybe@(just: x); };
}

