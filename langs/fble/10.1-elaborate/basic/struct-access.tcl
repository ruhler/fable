fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Pair@ = *(Bool@ a, Bool@ b);
  
  # Test elaboration with symbolic struct access
  (Pair@) { Bool@; } f = (Pair@ x) { x.b; };
  (Pair@) { Bool@; } g = $(f);

  g(Pair@(false, true)).true;
}
