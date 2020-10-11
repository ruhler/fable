fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Pair@ = *(Bool@ a, Bool@ b);
  
  # Test elaboration with symbolic struct value.
  (Bool@) { Pair@; } f = (Bool@ x) { Pair@(true, x); };
  (Bool@) { Pair@; } g = $(f);

  Pair@ tt = g(true);
  Unit@ _ = tt.a.true;
  Unit@ _ = tt.b.true;

  Pair@ tf = g(false);
  Unit@ _ = tf.a.true;
  Unit@ _ = tf.b.false;

  Unit@();
}
