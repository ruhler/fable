fble-test-error 9:3 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);
  Bool true = Bool@(true: Unit@());
  Bool false = Bool@(false: Unit@());

  # The union value type is not a union type.
  @ Pair = *(Bool just, Unit nothing);
  Pair@(just: true);
}
