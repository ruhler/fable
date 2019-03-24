fble-test-error 6:3 {
  @ Unit@ = *();
  <@>@ Maybe@ = <@ T@> { +(T@ just, Unit@ nothing); };

  # There are too few arguments to the poly type.
  Maybe@(nothing: Unit@());
}
