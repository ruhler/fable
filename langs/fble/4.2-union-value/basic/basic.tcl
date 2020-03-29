fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # Construct a basic union value.
  @ Maybe@ = +(Bool@ just, Unit@ nothing);
  Maybe@(just: true);
}
