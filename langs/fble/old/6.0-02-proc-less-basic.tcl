fble-test {
  # Test slightly less basic proc, including basic 
  # link, get, put, and exec.
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  Bool@ ~ get, put;
  Bool@ ignored := put(true);
  Bool@ result := get();
  $(result.true);
}
