fble-test-error 7:3 {
  @ Unit@ = *();

  Unit@ ~ get, put;

  # The type Bool@ is not defined.
  Bool@- myget = get;
  $(Unit@());
}
