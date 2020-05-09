fble-test-error 13:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  (Bool@) { (Bool@) { Bool@; }; } And = (Bool@ a) {
    a.?(true: (Bool@ b) { b; },
        false: (Bool@ b) { false; });
  };

  # Excessive application is not allowed.
  And(true, true).true;
}
