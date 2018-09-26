fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  @ Fruit@ = +(Unit@ apple, Unit@ banana, Unit@ cherry);

  # The two following function types are equal, even though they have
  # different argument names.
  @ FuncA@ = \(Bool@ x, Fruit@ y; Bool@);
  @ FuncB@ = \(Bool@ w, Fruit@ z; Bool@);

  FuncA@ a = \(Bool@ x, Fruit@ y) { true; };
  FuncB@ b = a;
  b;
}
