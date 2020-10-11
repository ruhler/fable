fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Maybe@ = +(Bool@ just, Unit@ nothing);
  
  # Test elaboration with symbolic union access
  (Maybe@) { Bool@; } f = (Maybe@ x) { x.just; };
  (Maybe@) { Bool@; } g = $(f);

  g(Maybe@(just: true)).true;
}
