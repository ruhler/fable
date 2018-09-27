fble-test-error 12:14 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  @ Fruit@ = +(Unit@ apple, Unit@ banana, Unit@ cherry);

  # The functions have different return types.
  @ FuncA@ = \(Bool@ x, Fruit@ y; Bool@);
  @ FuncB@ = \(Bool@ x, Fruit@ y; Fruit@);

  FuncA@ a = \(Bool@ x, Fruit@ y) { true; };
  FuncB@ b = a;
  b;
}
