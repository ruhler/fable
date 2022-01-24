fble-test {
  @ Unit@ = *();

  # Non-normal characters can be used in names when single quoted.
  Unit@ 'funny, name, this is!' = Unit@();
  'funny, name, this is!';
}
