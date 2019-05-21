fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  (Bool@) { Bool@; } Not = (Bool@ x) {
    ?(x; true: false, false: true);
  };

  # Function application of a single argument.
  Unit@ ff = Not(true).false;
  Unit@ tt = Not(false).true;
  Unit@();
}
