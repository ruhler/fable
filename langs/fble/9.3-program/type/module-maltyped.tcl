fble-test-compile-error 3:5 {
  % True = /Bool%.True;

  True.true;
} {
  Bool {
    # The value of the Bool module does not compile.
    zzz;
  } {}
}
