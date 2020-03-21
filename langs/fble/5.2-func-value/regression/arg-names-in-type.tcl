fble-test-error 9:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ True = Bool@(true: Unit@());

  # Regression test for a bug we had where we were freeing a type before
  # trying to print it in an error message when we accidentally put argument
  # names when specifying a function type.
  (Unit@ x) { Bool@; } f = (Unit@ x) {
    True;
  };

  f;
}
