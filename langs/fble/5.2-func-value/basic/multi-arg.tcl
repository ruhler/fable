fble-test {
  # A function value that has multiple argument.
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ false = Bool@(false: Unit@());

  (Bool@ x, Bool@ y) { ?(x; true: y, false: false); };
}

