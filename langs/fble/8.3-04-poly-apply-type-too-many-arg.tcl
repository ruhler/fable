fble-test-error 6:3 {
  @ Unit@ = *();
  <@>@ Maybe@ = \<@ T@> { +(T@ just, Unit@ nothing); };

  # There are too many arguments to the poly type.
  Maybe@<Unit@><Unit@>(nothing: Unit@());
}
