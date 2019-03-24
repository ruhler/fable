fble-test {
  @ Unit@ = *();

  <@>@ P@ = \<@ T@> { *(T@ head, S@<T@> tail); },
  <@>@ S@ = \<@ T@> { +(P@<T@> cons, Unit@ nil); };

  # Regression test of a poly application that results in itself.
  # There was a bug previously where this lead to a stack overflow.
  \<@ T@> { S@<T@>; } S0 = \<@ T@> {
    S@<T@>(nil: Unit@());
  };

  \<@ T@> {
    # Note: We use a let here to force a type check on S0<T@>.
    S@<T@> x = S0<T@>;
    x;
  };
}
