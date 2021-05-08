fble-test-runtime-error 0:0  {
  # The spec doesn't actually say anything about detecting deadlock, but the
  # reference implementation can detect deadlock in some cases and report
  # that, and it's nice if we can test it using the spec test infrastructure.
  #
  # TODO: This test really ought not to be part of the spec tests.
  @ Unit@ = *();
  Unit@ ~ get, put;
  Unit@ u := get;
  !(u);
}
