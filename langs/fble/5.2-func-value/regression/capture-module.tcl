fble-test {
  @ Unit@ = /Bool%.Unit@;
  @ Bool@ = /Bool%.Bool@;

  # Regression test for a bug we had where we were not properly capturing
  # module refs in function values.
  (Unit@) { Bool@; } f = (Unit@ _) {
    /Bool%.True;
  };

  f(Unit@()).true;
} {
  Bool {
    @ Unit@ = *();
    @ Bool@ = +(Unit@ true, Unit@ false);
    Bool@ True = Bool@(true: Unit@());
    Bool@ False = Bool@(false: Unit@());
    @(Unit@, Bool@, True, False);
  } {}
}
