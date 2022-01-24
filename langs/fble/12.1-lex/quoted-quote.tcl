fble-test {
  @ Unit@ = *();

  # Single quotes can be used in a name with consecutive single quotes.
  Unit@ 'can''t' = Unit@();
  'can''t';
}
