fble-test-error 6:12 {
  @ Unit@ = *();
  @ S1@ = *(Unit@ x);

  # The comma is missing between the two fields.
  +(S1@ a  Unit@ b);
}
