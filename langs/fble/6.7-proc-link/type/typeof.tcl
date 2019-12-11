fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  @ X@ = *(Unit@ x);

  # The type of a link port is the type of it's body. The type of the get port
  # is a process that returns the type of link. The type of the put port is a
  # function that takes a single argument of the link type and returns a unit
  # process.
  X@! x = {
    Bool@ ~ get, put;
    Bool@! g = get;
    (Bool@){ Unit@!; } p = put;
    $(X@(Unit@()));
  }; x;
}
