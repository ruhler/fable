fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ True = Bool@(true: Unit);
  Bool@ False = Bool@(false: Unit);

  # It's fine if letters contains a bunch of extra unrelated fields.
  @ Enum@ = +(Unit@ A, Bool@ x, Unit@ B, Unit@ C, Unit@ abc);

  @ L@ = +(*(Enum@ head, L@ tail) cons, Unit@ nil);

  (L@) { Unit@; } f = (L@ l) {
    Unit@ _b = l.cons.head.B;
    Unit@ _a = l.cons.tail.cons.head.A;
    Unit@ _c = l.cons.tail.cons.tail.cons.head.C;
    Unit@ _e = l.cons.tail.cons.tail.cons.tail.nil;
    Unit;
  };

  f|BAC;
}
