fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ True = Bool@(true: Unit);
  Bool@ False = Bool@(false: Unit);

  (Bool@) { Bool@; } Id = (Bool@ x) { x; };
  (Bool@, Bool@) { Bool@; } Cons = (Bool@ a, Bool@ b) { True; };
  Bool@ Nil = False;

  % L = @('|': Id, ',': Cons, '': Nil);

  # Empty lists are allowed.
  L[].false;
}
