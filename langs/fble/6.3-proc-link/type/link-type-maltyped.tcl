fble-test-error 5:3 {
  @ Unit@ = *();

  # The type Bool@ has not been defined.
  Bool@ ~ get, put;
  $(Unit@());
}
