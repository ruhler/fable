fble-test {
  # TODO: Uncomment the following to re-enable this test
  xxx

  @ Unit@ = *();

  <@>@ P@ = <@ T@> { *(T@ head, S@<T@> tail); },
  <@>@ S@ = <@ T@> { +(P@<T@> cons, Unit@ nil); };

  # Regression test of a poly application that results in itself.
  # There was a bug previously where this was a type error.
  S@ S0 = <@ T@> {
    S@<T@>(nil: Unit@());
  };

  S0;
}
