fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);

  @ L@ = +(*(Enum@ head, L@ tail) cons, Unit@ nil);

  (L@) { Unit@; } f = (L@ l) {
    Unit@ _b = l.cons.head.B;
    Unit@ _a = l.cons.tail.cons.head.A;
    Unit@ _c = l.cons.tail.cons.tail.cons.head.C;
    Unit@ _e = l.cons.tail.cons.tail.cons.tail.nil;
    Unit;
  };

  # Basic use of a list expression.
  Unit@ _ = f[Enum@(B: Unit), Enum@(A: Unit), Enum@(C: Unit)];
  Unit;
}
