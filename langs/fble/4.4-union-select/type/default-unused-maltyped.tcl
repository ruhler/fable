fble-test-error 17:10 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  @ Char@ = +(Unit@ a, Unit@ b, Unit@ c, Unit@ d, Unit@ e);

  # The default branch isn't used here, because we've provided all the tags.
  # Test that errors in an unused default branch are considered errors in the
  # pogram.
  Char@(b: Unit@()).?(
      a: true,
      b: true,
      c: true,
      d: true,
      e: true,
       : zzz).true;
}
