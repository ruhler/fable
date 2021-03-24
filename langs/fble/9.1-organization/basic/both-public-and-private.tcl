fble-test-error 3:5 {
  # A module is not allowed to be both public and private.
  { /A%; };
} {
  A {
    @();
  }
} {
  "A*" {
    @();
  }
}
