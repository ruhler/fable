fble-test {
  @ Unit@ = *();

  # Variables have the type they are declared to have.
  Unit@ x = Unit@();
  Unit@ y = x;
  y;
}
