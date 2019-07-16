fble-test-error 6:20 {
  @ Unit@ = *();

  # A field called y@ should take on values of a type, but here it is taking
  # on union values.
  *(Unit@ x, Unit@ y@);
}
