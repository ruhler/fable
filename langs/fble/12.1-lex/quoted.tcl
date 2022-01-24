fble-test {
  @ Unit@ = *();

  # unit and 'unit' refer to the same entity. We should be able to use them
  # interchangeably.
  Unit@ unit = Unit@();
  'unit';
}
