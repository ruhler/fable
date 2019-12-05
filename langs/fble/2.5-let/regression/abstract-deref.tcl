fble-test-error 7:14 {
  @ Unit@ = *();
  @ Nat@ = *(Unit@ Z, Nat@ S);

  # This is a fairly nonsense let binding. It should give a reasonable error
  # message at least.
  Nat@ n = n.S;
  n.Z;
}
