fble-test-compile-error 5:28 {
  @ Unit@ = *();

  # There are too many arguments to the type variable T.
  <@>@ Maybe@ = <@ T@> { +(T@<Unit@> just, Unit@ nothing); };
  Maybe@<Unit@>(nothing: Unit@());
}
