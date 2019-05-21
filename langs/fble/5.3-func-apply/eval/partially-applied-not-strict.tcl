fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  (Bool@, Bool@) { Bool@; } And = (Bool@ a, Bool@ b) {
    Unit@ tf = true.false;
    ?(a; true: b, false: Bool@(false: tf));
  };

  # The multi argument function is not fully applied given one argument.
  # The function body is not evaluated before all arguments are provided.
  And(true);
}
