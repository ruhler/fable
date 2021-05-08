fble-test-compile-error 3:6 {
  # The kind is malformed.
  <@,blah>@ Id@ = <@ A@> { A@; };
  <@ B@> { Id@<B@>; };
}
