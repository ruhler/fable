fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit);
  Bool@ f = Bool@(false: Unit);

  @ Char@ = +(Unit@ a, Unit@ b, Unit@ c, Unit@ d, Unit@ e);

  # Basic test of statement syntax for union select with default values.
  {
    t.?(true: {
      f.?(true: {
        Char@(a: Unit);
      });
      Char@(b: Unit);
    });

    f.?(true: {
      Char@(c: Unit);
    });

    Char@(d: Unit);
  }.b;
}
