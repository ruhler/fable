fble-test-error 6:3 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);

  # The object of the access is not a struct or union value.
  Bool.just;
}
