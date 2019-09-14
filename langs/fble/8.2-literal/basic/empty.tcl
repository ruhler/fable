fble-test {
  @ Unit@ = *();
  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);

  # Empty string literals are allowed
  <@ L@>((Enum@, L@){L@;}, L@){L@;} f = Enum@|''; 

  @ P@ = { *(Enum@ head, S@ tail); },
  @ S@ = { +(P@ cons, Unit@ nil); };

  (Enum@, S@){S@;} Cons = (Enum@ x, S@ xs) {
    S@(cons: P@(x, xs));
  };

  S@ Nil = S@(nil: Unit@());

  S@ l = f<S@>(Cons, Nil);

  l.nil;
}
