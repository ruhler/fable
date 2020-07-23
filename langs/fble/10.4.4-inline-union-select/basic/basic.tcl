fble-test {
  @ Unit@ = *$();
  @ Bool@ = +$(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # Basic test of inline union select.
  Bool@ z = f.?(true: f, false: t);
  $(z).true;
}
