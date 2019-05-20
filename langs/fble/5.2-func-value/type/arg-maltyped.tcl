fble-test-error 7:4 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ Maybe@ = +(Bool@ just, Unit@ nothing);

  # The argument type is not valid.
  (zzz@ x) { Maybe@(just: x); };
}

