fble-test {
  # Test a polymorphic type with multiple type arguments.
  <@, @>@ Pair@ = <@ A@, @ B@> { *(A@ first, B@ second); };

  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  Pair@<Unit@, Bool@> x = Pair@<Unit@, Bool@>(Unit@(), true);
  x.second.true;
}
