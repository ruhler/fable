fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # It should be fine to put a value on a link that is never consumed.
  Bool@ ~ get, put;
  put(true);
}
