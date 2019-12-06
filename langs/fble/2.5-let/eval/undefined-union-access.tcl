fble-test-error 7:12 {
  @ Unit@ = *();
  @ T@ = +(Unit@ nil, T@ tail);

  # The value of 't' is undefined in the let bindings. This will produce
  # undefined behavior when accessing the field tail of the undefined value t.
  T@ t = t.tail;
  Unit@();
}
