fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  
  # Basic test of elaboration.
  (Bool@) { Bool@; } f = (Bool@ x) { x; };
  (Bool@) { Bool@; } g = $(f);
  g(Bool@(false: Unit@())).false;
}
