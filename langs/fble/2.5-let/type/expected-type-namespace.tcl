fble-test-compile-error 4:5 {
  # Unit has kind @, so it should be in the type namespace, not the normal
  # namespace. e.g. it should be named Unit@ instead.
  @ Unit = *();
  Unit;
}
