fble-test {
  # Test a polymorphic struct type.
  <@>@ Pair@ = <@ T@> { *(T@ first, T@ second); };

  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  Pair@<Bool@> x = Pair@<Bool@>(false, true);
  x.second.true;
}
