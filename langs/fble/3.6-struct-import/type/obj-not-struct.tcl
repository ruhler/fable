fble-test-error 8:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ Either@ = +(Bool@ x, Bool@ y);
  Either@ s = Either@(x: Bool@(true: Unit@()));

  # The object is not a struct object.
  s; x.true;
}
