fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Pair@ = *(Bool@ first, Bool@ second);

  (Pair@)((Bool@){ Bool@; }) { Pair@; } Map = (Pair@ p)((Bool@){ Bool@; } f) {
    Pair@(f(p.first), f(p.second));
  };

  # Use function bind syntax to apply an anonymous function to both elements
  # of a pair.
  Pair@ p = {
    Bool@ b <- Map(Pair@(true, false));
    b.?(true: false, false: true);
  };

  Unit@ _ = p.first.false;
  Unit@ _ = p.second.true;
  Unit@();
}
