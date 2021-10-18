fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);

  # It's fine to pick whatever field names you like.
  @ L@ = +(*(Enum@ a, L@ b) c, Unit@ d);

  (L@) { Unit@; } f = (L@ l) {
    Unit@ _b = l.c.a.B;
    Unit@ _a = l.c.b.c.a.A;
    Unit@ _c = l.c.b.c.b.c.a.C;
    Unit@ _e = l.c.b.c.b.c.b.d;
    Unit;
  };

  Unit@ _ = f[Enum@(B: Unit), Enum@(A: Unit), Enum@(C: Unit)];
  Unit;
}
