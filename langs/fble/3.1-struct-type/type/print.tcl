fble-test-error 7:27 {
  @ Unit@ = *();

  # Cause an unlabeled struct type to be printed in an error message. We don't
  # test that it actually prints something meaningful, but this increases code
  # coverage and at least will make sure we don't crash in this case.
  *(Unit@ a, Unit@ b) x = Unit@();
  x;
}
