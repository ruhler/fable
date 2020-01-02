fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # Proc eval is non-strict: the argument is not evaluated until we run the
  # process itself.
  Unit@! mku = $(true.false);
  Unit@();
}

