fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Units@ = *(Unit@ a, Unit@ b);

  # It should be fine to put multiple values on a link before getting them,
  # and they should come out in proper order.
  Bool@ ~ get, put;
  Bool@ i0 := put(true);
  Bool@ i1 := put(false);
  Bool@ t := get();
  Bool@ f := get();
  $(Units@(t.true, f.false));
}
