fble-test {
  @ UnitE@ = *();
  @ BoolE@ = +(UnitE@ true, UnitE@ false);

  @ UnitF@ = *$();
  @ BoolF@ = +$(UnitF@ true, UnitF@ false);

  BoolF@ TrueF = BoolF@(true: UnitF@());

  # Test basic use of inline value evaluation.
  BoolE@ TrueE = $(TrueF);
  TrueE.true;
}
