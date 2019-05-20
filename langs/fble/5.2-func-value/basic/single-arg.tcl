fble-test {
  # A function value that has a single argument.
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ Maybe@ = +(Bool@ just, Unit@ nothing);

  (Bool@ x) { Maybe@(just: x); };
}

