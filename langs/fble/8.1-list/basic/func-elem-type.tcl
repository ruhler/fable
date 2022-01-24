fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ True = Bool@(true: Unit);
  Bool@ False = Bool@(true: Unit);

  # It's fine for the element type to be anything, including functions.
  @ E@ = (Bool@) { Bool@; };

  @ L@ = +(*(E@ head, L@ tail) cons, Unit@ nil);

  (L@) { Unit@; } f = (L@ l) {
    Unit@ _id = l.cons.head(True).true;
    Unit@ _not = l.cons.tail.cons.head(False).true;
    Unit;
  };

  f[(Bool@ x) { x; }, (Bool@ x) { x.?(true: False, false: True); } ];
}
