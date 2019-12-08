fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # The type of a put is a process that returns the Unit type.
  (Bool@+ output) {
    Unit@! doput = output(true);
    Unit@();
  };
}
