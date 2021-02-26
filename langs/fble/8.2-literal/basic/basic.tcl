fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();
  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);

  @ P@ = { *(Enum@ head, S@ tail); },
  @ S@ = { +(P@ cons, Unit@ nil); };

  S@ Nil = S@(nil: Unit@());

  (Enum@, S@) { S@; } Cons = (Enum@ x, S@ xs) {
    S@(cons: P@(x, xs));
  };

  (S@) { S@; } Id = (S@ s) { s; };

  % Letters = @(A: Enum@(A: Unit), B: Enum@(B: Unit), C: Enum@(C: Unit));
  % L = @('|': Id, ',': Cons, '': Nil, '?': Letters);

  # Basic use of a literal expression.
  S@ l = L|BAC;
  Unit@ _b = l.cons.head.B;
  Unit@ _a = l.cons.tail.cons.head.A;
  Unit@ _c = l.cons.tail.cons.tail.cons.head.C;
  Unit@ _e = l.cons.tail.cons.tail.cons.tail.nil;
  Unit@();
}
