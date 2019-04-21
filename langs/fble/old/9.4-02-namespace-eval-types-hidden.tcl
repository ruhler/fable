fble-test-error 9:5 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  Bool@ foo = Bool@(true: Unit@());

  Bool@ bar = @(true: Bool@(true: Unit@()), false: Bool@(false: Unit@())) {
    # The type Bool@ should not be visible here.
    Bool@ foo = false;
    foo;
  };

  Unit@ tt = foo.true;
  Unit@ ff = bar.false;
  Unit@();
}
