fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # There's no difference between a multi-arg function and the corresponding
  # nested single argument function type.
  @ A@ = (Unit@, Bool@) { Bool@; };
  @ B@ = (Unit@) { (Bool@) { Bool@;}; };
  A@ z = (Unit@ x) { (Bool@ y) { true; }; };
  z;
}
