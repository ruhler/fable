fble-test {
  @ Unit@ = *$();
  @ Bool@ = +$(Unit@ true, Unit@ false);

  # Construct a basic inline union value.
  Bool@(true: Unit@());
}
