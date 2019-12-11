fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # This is a regression test for a bug we had where even though the type of
  # put was Unit@!, at runtime we would return the putted value instead of
  # Unit.
  Bool@ ~ get, put;
  Unit@ a := put(true);

  # Do a namespace import on the returned value to force inspection of it.
  a;
  $(Unit@());
}
