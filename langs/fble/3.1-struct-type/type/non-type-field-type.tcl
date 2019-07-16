fble-test-error 5:23 {
  @ Unit@ = *();

  # A field called y should not take on values of a type.
  *(Unit@ x, @<Unit@> y);
}
