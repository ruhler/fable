fble-test-error 5:3 {
  @ Unit@ = *();

  # The type Bool@ is not defined.
  !Bool@ p = $(Unit@());
  p;
}
