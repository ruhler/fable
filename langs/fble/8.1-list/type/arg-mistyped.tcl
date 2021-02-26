fble-test-error 19:24 {
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

  # All arguments to the list must have the same type.
  L[Enum@(B: Unit@()), Unit@(), Enum@(C: Unit@())];
}
