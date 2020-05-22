fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Units@ = *(Unit@ a, Unit@ b);

  # Test exec with multiple processes.
  Bool@ t := !(true), Bool@ f := !(false);
  !(Units@(t.true, f.false));
}
