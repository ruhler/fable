fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);

  (Bool@, Bool@) { Unit@; } Callee = (Bool@ a, Bool@ b) {
    a.?(
      true: b.?(true: Unit, false: Unit),
      false: b.?(true: Unit, false: Unit));
  };

  (Unit@) { Unit@; } Caller = (Unit@ u) {
    # Regression test for an issue we had in the past tracking lifetimes of
    # arguments to a tail call if the same local variable was used for
    # multiple different arguments of the call.
    Bool@ t = Bool@(true: Unit);
    Callee(t, t);
  };

  Caller(Unit);
}
