fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Maybe@ = +(Bool@ just, Unit@ nothing);
  
  # Test elaboration with symbolic union value.
  (Bool@) { Maybe@; } f = (Bool@ x) { Maybe@(just: x); };
  (Bool@) { Maybe@; } g = $(f);

  Maybe@ mt = g(true);
  Unit@ _ = mt.just.true;

  Maybe@ mf = g(false);
  Unit@ _ = mf.just.true;

  Unit@();
}
