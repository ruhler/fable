fble-test {
  # A union type that has two fields.
  @ Unit@ = *();
  @ S1@ = *(Unit@ x);
  +(S1@ a, Unit@ b);
}
