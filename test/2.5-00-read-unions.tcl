set prg {
  struct Unit();
  union Bool(Unit True, Unit False);

  proc main(Bool <~ in ; ; Bool) {
    Bool x = ~in();
    ~in();
  };
}

# Verify we can read two union values in a row. There was a bug in the past
# where we failed to check for the close paren at the end of a union type.
fblc-test Bool:True(Unit()) $prg main {} {
  put in Bool:False(Unit())
  put in Bool:True(Unit())
}

