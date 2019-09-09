fble-test {
  @ Unit@ = *();
  @ Enum@ = *(Unit@ A, Unit@ B, Unit@ C);

  # Basic use of a list expression.
  <@ L@>((Enum@, L@){L@;}, L@){L@;} f = [
    Enum@(B: Unit@()),
    Enum@(A: Unit@()),
    Enum@(C: Unit@())];

  @ P@ = { *(Enum@ head, S@ tail); },
  @ S@ = { +(P@ cons, Unit@ nil); };

  (Enum@, S@){S@;} Cons = (Enum@ x, S@ xs) {
    S@(cons: P@(x, xs));
  };

  S@ Nil = S@(nil: Unit@());

  S@ l = f<Enum@>(Cons, Nil);

  Unit@ b = l.cons.head.B;
  Unit@ a = l.cons.tail.cons.head.A;
  Unit@ c = l.cons.tail.cons.tail.cons.head.C;
  Unit@ e = l.cons.tail.cons.tail.cons.tail.nil;
  Unit@();
}
