fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ Maybe@ = +(Bool@ just, Unit@ nothing);

  # The type of a function value is its function type.
  (Bool@) { Maybe@; } f = (Bool@ x) { Maybe@(just: x); };
  f;
}

