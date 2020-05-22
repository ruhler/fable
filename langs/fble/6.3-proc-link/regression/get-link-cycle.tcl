fble-test {
  # Regression test where we had not yet implemented support for deleting
  # references that could break cycles.
  @ Unit@ = *();

  Unit@! ~ get, put;
  Unit@ _ := put({
    # Put a Unit@! on the link that references the get port, which itself
    # references the link, to create a reference cycle on the link with
    # itself.
    Unit@! x := get;
    x;
  });

  # Take the value off the link, to break the reference cycle.
  Unit@! _ := get;

  !(Unit@());
}

