fble-test-error 5:32 {
  @ Unit@ = *();

  # There are too many arguments to the type variable T.
  \<@; @> Maybe@ = \<@ T@> { +(T@<Unit@> just, Unit@ nothing); };
  Maybe@<Unit@>(nothing: Unit@());
}
