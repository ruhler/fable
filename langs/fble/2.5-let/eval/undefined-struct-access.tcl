fble-test-runtime-error 7:14 {
  @ Unit@ = *();
  @ Nat@ = *(Unit@ Z, Nat@ S);

  # The value of 'n' is undefined in the let bindings. This will produce
  # undefined behavior when accessing the field S of the undefined value n.
  Nat@ n = n.S;
  Unit@();
}
