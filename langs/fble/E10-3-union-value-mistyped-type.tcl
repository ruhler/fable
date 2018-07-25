fble-test-error 9:3 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);
  Bool true = Bool@(true: Unit@());
  Bool false = Bool@(false: Unit@());

  # The union value type is not a type
  @ Maybe = +(Bool just, Unit nothing);
  true@(just: true);
}
