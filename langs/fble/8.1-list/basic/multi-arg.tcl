fble-test {
  @ Unit@ = *();
  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);

  @ P@ = { *(Enum@ head, S@ tail); },
  @ S@ = { +(P@ cons, Unit@ nil); };

  S@ Nil = S@(nil: Unit@());

  (Enum@, S@) { S@; } Cons = (Enum@ x, S@ xs) {
    S@(cons: P@(x, xs));
  };

  (S@) { S@; } Id = (S@ s) { s; };

  % L = @('|': Id, ',': Cons, '': Nil);

  # Basic use of a list expression.
  S@ l = L[Enum@(B: Unit@()), Enum@(A: Unit@()), Enum@(C: Unit@())];

  Unit@ _b = l.cons.head.B;
  Unit@ _a = l.cons.tail.cons.head.A;
  Unit@ _c = l.cons.tail.cons.tail.cons.head.C;
  Unit@ _e = l.cons.tail.cons.tail.cons.tail.nil;
  Unit@();
}
