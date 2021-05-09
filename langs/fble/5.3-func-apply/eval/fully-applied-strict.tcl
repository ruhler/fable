fble-test-runtime-error 7:21 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  (Bool@) { (Bool@) { Bool@; }; } And = (Bool@ a) {
    Unit@ tf = true.false;
    a.?(true: (Bool@ b) { b; },
        false: (Bool@ b) { Bool@(false: tf); });
  };

  # The single argument function is fully applied given one argument.
  And(true);
}
