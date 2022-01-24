fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);

  @ L@ = +(*(Enum@ head, L@ tail) cons, Unit@ nil);

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ True = Bool@(true: Unit);
  Bool@ False = Bool@(false: Unit);

  (L@) { Bool@; } IsEmpty = (L@ l) {
    l.?(cons: False, nil: True);
  };

  # Empty literals are allowed.
  Unit@ _ = IsEmpty|''.true;
  Unit@ _ = IsEmpty|'ABB'.false;
  Unit;
}
