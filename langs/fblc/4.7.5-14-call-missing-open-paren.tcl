set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub( ; ; Bool) {
    $(Bool:true(Unit()));
  };

  proc main( ; ; Bool) {
    # The open parenthesis is missing.
    sub ; );
  };
}
fblc-check-error $prg 11:9
