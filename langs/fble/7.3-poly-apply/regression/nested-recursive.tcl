fble-test {
  # Regression test for when we had an infinite loop trying to test type
  # equality when doing nested application in a recursively defined poly.

  # Here's the real motivating example:
  #<@>@ Map@ = <@ T@>{ +(Unit@ empty, MapP@<T@> map); },
  #<@>@ MapP@ = <@ T@>{ *(Unit@ unit, Map@<Map@<T@>> product); };

  # Here's a minimized example:
  <@>@ M@ = <@ T@>{ *(M@<M@<T@>> m); };
  
  *();
}
