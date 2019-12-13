fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # Recursive processes are allowed.
  Bool@ ~ get, put;
  Unit@! p = {
    Bool@ b := get;
    ?(b; true: p, false: $(Unit@()));
  };
  Unit@ _ := put(Bool@(true: Unit@()));
  Unit@ _ := put(Bool@(true: Unit@()));
  Unit@ _ := put(Bool@(false: Unit@()));
  p;
}
