fble-test-error 8:23 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # Proc eval is strict: the argument is evaluated before we run the process
  # itself.
  Unit@! mku = $(true.false);
  Unit@();
}

