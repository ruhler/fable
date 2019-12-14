fble-test-error 3:19 {
  { Bool%; @(True); };

  True;
} {
  Bool {
    { Unit%; @(Unit@, Unit); };
    @ Bool@ = +(Unit@ true, Unit@ false);
    Bool@ True = Bool@(true: Unit);
    @(Bool@, True);
  } { {
      Unit {
        # The module Foo% does not exist.
        @ Unit@ = Foo%;
        Unit@ Unit = Unit@();
        @(Unit@, Unit);
      } {}
    }
  }
}
