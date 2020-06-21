fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ True = Bool@(true: Unit);
  Bool@ False = Bool@(false: Unit);

  False.?(
    true: {
      # We had a bug in the past where we failed to release a function after
      # applying it. The bug exhibited as an attempt to release a null value:
      # we thought the function Id was still alive in the false branch of the
      # union select and tried to release it before returning. But it's not even
      # in scope in the false branch of the union_select.
      (Bool@) { Bool@; } Id = (Bool@ x) { x; };
      Id(True);
    },
    false: True);
}
