fble-test-error 6:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # The port argument is not in scope.
  zzz();
}


