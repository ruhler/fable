fble-test-error 8:20 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  (Bool@! output) {
    # The port type is for a Bool@, not a Unit@.
    Unit@! doget = output();
    Unit@();
  };
}


