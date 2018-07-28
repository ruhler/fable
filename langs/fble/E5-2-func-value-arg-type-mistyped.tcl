fble-test-error 8:13 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);
  Bool true = Bool@(true: Unit@());
  Bool false = Bool@(false: Unit@());

  # The second argument's type is not a type.
  \(Bool x, Unit@() y) {
    ?(x; true: true, false: y);
  };
}
