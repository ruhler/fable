fble-test {
  @ Unit@ = *$();
  @ Bool@ = +$(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # Basic inline union access.
  Unit@ u = true.true;
  $(u);
}
