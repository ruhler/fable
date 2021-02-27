fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ True = Bool@(true: Unit);
  Bool@ False = Bool@(false: Unit);

  @ Enum@ = +(Unit@ A, Unit@ B);
  Enum@ A = Enum@(A: Unit);
  Enum@ B = Enum@(B: Unit);

  @ EnumSet@ = *(Bool@ A, Bool@ B);

  # Test that the apply function can transform the type of the literal.
  (EnumSet@) { Bool@; } HasBoth = (EnumSet@ s) {
    s.A.?(true: s.B, false: False);
  };

  (Enum@, EnumSet@) { EnumSet@; } Cons = (Enum@ e, EnumSet@ s) {
    e.?(A: EnumSet@(True, s.B), B: EnumSet@(s.A, True));
  };

  EnumSet@ Nil = EnumSet(False, False);

  # L here represents a literal that has the value True if both A and B are
  # included in the list, False otherwise.
  % L = @('|': HasBoth, ',': Cons, '': Nil);

  Unit@ _ = L[A, A, A].false;
  Unit@ _ = L[A, B, A].true;
  Unit@ _ = L[B, B, B].false;
  Unit;
}
