fble-test-error 3:13 {
  # The definition for Pair@ has the wrong kind.
  @ Pair@ = \<@ T@> {
    *(T@ first, T@ second);
  };

  @ Unit@ = *();
  Pair@<Unit@>(Unit@(), Unit@());
}
