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

  # It's fine if letters contains a bunch of extra unrelated fields.
  % Letters = @(
    A: Enum@(A: Unit),
    x: Unit,
    B: Enum@(B: Unit),
    C: Enum@(C: Unit),
    id: (Unit@ i) { i; }
  );

  % L = @('|': Id, ',': Cons, '': Nil, '?': Letters);

  S@ l = L|BAC;
  Unit@ _b = l.cons.head.B;
  Unit@ _a = l.cons.tail.cons.head.A;
  Unit@ _c = l.cons.tail.cons.tail.cons.head.C;
  Unit@ _e = l.cons.tail.cons.tail.cons.tail.nil;
  Unit@();
}
