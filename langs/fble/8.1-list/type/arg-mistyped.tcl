fble-test-error 14:21 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);

  @ L@ = +(*(Enum@ head, L@ tail) cons, Unit@ nil);

  (L@) { Unit@; } f = (L@ l) {
    Unit;
  };

  # All arguments to the list must have the same type.
  f[Enum@(B: Unit), Unit, Enum@(C: Unit)];
}
