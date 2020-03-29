fble-test-error 4:5 {
  # Unit@ has kind %, so it should be in the normal namespace, not the type
  # namespace. e.g. it should be named Unit instead.
  % Unit@ = *()();
  Unit@;
}
