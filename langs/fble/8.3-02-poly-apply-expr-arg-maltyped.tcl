fble-test-error 7:11 {
  @ Unit@ = *();
  <@>@ Maybe@ = \<@ T@> { +(T@ just, Unit@ nothing); };
  Maybe@ Nothing = \<@ T@> { Maybe@<T@>(nothing: Unit@()); };

  # The argument to the poly expr is undefined.
  Nothing<zzz@>;
}
