fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # Test exec with a single process.
  Bool@ t := $(true);
  $(t.true);
}
