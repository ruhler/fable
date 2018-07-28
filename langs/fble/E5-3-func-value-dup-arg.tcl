fble-test-error 8:18 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);
  Bool true = Bool@(true: Unit@());
  Bool false = Bool@(false: Unit@());

  # The second argument has the same name as the first arg.
  \(Bool x, Bool x) {
    ?(x; true: false, false: true);
  };
}
