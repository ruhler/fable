fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # The type of a a get is a process that returns the type on the input port.
  (Bool@- input) {
    Bool@! doget = input();
    Unit@();
  };
}


