

set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    // The variable y is not declared.
    Bool x = $(y);
    $(x);
  };
}

fblc-check-error $prg

