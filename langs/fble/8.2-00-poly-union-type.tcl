fble-test {
  # Test a basic use of polymorphic types.
  @ Unit@ = *();
  <@>@ Maybe@ = \<@ T@> { +(T@ just, Unit@ nothing); };

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  Maybe@<Bool@> mb = Maybe@<Bool@>(just: true);
  mb.just.true;
}
