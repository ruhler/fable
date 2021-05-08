fble-test-runtime-error 9:8 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # We had a bug in the past where we assumed that all poly expressions are
  # the last expressions in their block.
  <@>@ Maybe@ = <@ T@> { +(T@ just, Unit@ nothing); };
  true.false;
}
