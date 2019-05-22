fble-test {
  @ Unit@ = *();

  # The two process types are equal.
  Unit@! x = $(Unit@());
  Unit@! y = x;
  x;
}
