fble-test {
  # A function type that has multiple arguments.
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ Maybe@ = +(Bool@ just, Unit@ nothing);
  (Bool@, Unit@) { Maybe@; };
}
