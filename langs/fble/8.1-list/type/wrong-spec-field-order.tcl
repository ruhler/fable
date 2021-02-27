fble-test-error 20:3 {
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

  # The order of the fields is incorrect. '|' should be before ','.
  % L = @(',': Cons, '|': Id, '': Nil, '?': Unit);

  L[Enum@(B: Unit), Enum@(A: Unit), Enum@(C: Unit)];
}
