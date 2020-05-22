fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # The type of eval is the process type of the type of the argument.
  Bool@! mkt = !(true);
  mkt;
}

