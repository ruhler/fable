fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();
  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);

  @ P@ = { *(Enum@ head, S@ tail); },
  @ S@ = { +(P@ cons, Unit@ nil); };

  S@ Nil = S@(nil: Unit);

  (Enum@, S@) { S@; } Cons = (Enum@ x, S@ xs) {
    S@(cons: P@(x, xs));
  };

  (S@) { S@; } Id = (S@ s) { s; };

  % L = @('|': Id, ',': Cons, '': Nil, '?': Unit);

  # Regression test for a bug we had in the past keeping track of variables
  # defined within the list literal.
  S@ l = L[
    Enum@(B: Unit),
    {
      % x = @('a': Unit, 'b': Unit, 'c': Unit, 'd': Unit, 'e': Unit, 'f': Unit);
      Enum@(A: x.f);
    },
    Enum@(C: Unit)];

  Unit@ _b = l.cons.head.B;
  Unit@ _a = l.cons.tail.cons.head.A;
  Unit@ _c = l.cons.tail.cons.tail.cons.head.C;
  Unit@ _e = l.cons.tail.cons.tail.cons.tail.nil;
  Unit;
}
