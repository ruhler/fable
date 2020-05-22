fble-test {
  # Regression test where we were not properly tracking references for links,
  # so that a link that was in a reference cycle with itself would never be
  # freed.
  @ Unit@ = *();

  Unit@! ~ get, put;
  Unit@ _ := put({
    # Put a Unit@! on the link that references the get port, which itself
    # references the link, to create a reference cycle on the link with
    # itself.
    Unit@! x := get;
    x;
  });
  !(Unit@());
}
