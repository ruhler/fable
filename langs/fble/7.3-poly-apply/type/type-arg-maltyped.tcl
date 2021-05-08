fble-test-compile-error 6:10 {
  @ Unit@ = *();
  <@>@ Maybe@ = <@ T@> { +(T@ just, Unit@ nothing); };

  # The argument to the poly type is undefined.
  Maybe@<zzz@>(nothing: Unit@()).nothing;
}
