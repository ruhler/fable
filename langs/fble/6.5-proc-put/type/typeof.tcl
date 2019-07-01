fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # The type of a put is a process that returns the type on the output port.
  (Bool@+ input) {
    Bool@! doget = input(true);
    Unit@();
  };
}


