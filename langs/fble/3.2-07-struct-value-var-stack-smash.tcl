fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ Foo@ = *(Bool@ a, Bool@ b, Bool@ c, Bool@ d, Bool@ e);

  Bool@ true = Bool@(true: Unit@());

  # Regression test that we correctly maintain the variable stack when pushing
  # temporary values for struct arguments.
  Foo@(Bool@(false: Unit@()), Bool@(false: Unit@()), true, 
       Bool@(false: Unit@()), Bool@(false: Unit@())).c.true;
}
