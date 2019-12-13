fble-test {
  @ Unit@ = *();
  @ A@ = *(Unit@ x, Unit@ y);
  @ B@ = *(Unit@ w, A@ a, Unit@ z);

  (Unit@) { A@; } f = (Unit@ v) {
    A@(v, Unit@());
  };

  # Test that we can still refer to variables in scope after a function call.
  Unit@ v = Unit@();
  B@(v, f(v), v);
}
