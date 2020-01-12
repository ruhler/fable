fble-test {
  @ Unit@ = *();

  # TODO: Uncomment the following line to re-enable the test.
  ???

  # Regression test for when we had an infinite loop trying to test type
  # equality when doing nested application in a recursively defined poly.
  <@>@ Map@ = <@ T@>{ +(Unit@ empty, MapP@<T@> map); },
  <@>@ MapP@ = <@ T@>{ *(Unit@ unit, Map@<Map@<T@>> product); };
  
  Unit@();
}
