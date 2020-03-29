fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);
  % Enums = @(A: Enum@(A: Unit), B: Enum@(B: Unit), C: Enum@(C: Unit));

  # Basic use of a literal expression.
  <@ L@>((Enum@, L@){L@;}, L@){L@;} f = Enums|BAC; 

  @ P@ = { *(Enum@ head, S@ tail); },
  @ S@ = { +(P@ cons, Unit@ nil); };

  (Enum@, S@){S@;} Cons = (Enum@ x, S@ xs) {
    S@(cons: P@(x, xs));
  };

  S@ Nil = S@(nil: Unit@());

  S@ l = f<S@>(Cons, Nil);

  Unit@ _b = l.cons.head.B;
  Unit@ _a = l.cons.tail.cons.head.A;
  Unit@ _c = l.cons.tail.cons.tail.cons.head.C;
  Unit@ _e = l.cons.tail.cons.tail.cons.tail.nil;
  Unit@();
}
