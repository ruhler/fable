fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ false = Bool@(false: Unit@());

  @ Maybe@ = +(Bool@ just, Unit@ nothing);
  
  # Test elaboration with symbolic union select
  (Maybe@) { Bool@; } f = (Maybe@ x) {
    x.?(just: x.just, nothing: false);
  };

  (Maybe@) { Bool@; } g = $(f);

  g(Maybe@(nothing: Unit@())).false;
}
