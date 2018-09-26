fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  @ Fruit@ = +(Unit@ apple, Unit@ banana, Unit@ cherry);

  # The two following function types are equal.
  @ FuncA@ = \(Bool@ x, Fruit@ y; Bool@);
  @ FuncB@ = \(Bool@ x, Fruit@ y; Bool@);

  FuncA@ a = \(Bool@ x, Fruit@ y) { true; };
  FuncB@ b = a;
  b;
}
