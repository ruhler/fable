fble-test-error 4:12 {
  <@ A@, @B@> (A@ a) {
    # A and B are type variables refering to different types.
    B@ x = a;
    x;
  };
}
