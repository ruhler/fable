fble-test-error 11:14 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The default argument has the wrong type.
  # TODO: Where should the error message be? The current implementation uses
  # the default value as the reference type, but that gives a funny location
  # for the error message.
  ?(f; true: f, : Unit@());
}
