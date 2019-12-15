fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # Take typeof a type.
  @<Bool@> Foo@ = Bool@;

  Foo@ True = Foo@(true: Unit@());
  True.true;
}
