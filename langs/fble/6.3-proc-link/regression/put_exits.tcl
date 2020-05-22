fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # Regression test for when you do a put as the last thing in a function.
  # This was causing an internal assertion failure because we didn't properly
  # handle the exit condition in the special primitive put function.
  Bool@ ~ get, put;
  (Bool@) { Unit@!; } f = (Bool@ b) {
    put(b);
  };
  Unit@ u := f(true);
  !(u);
}
