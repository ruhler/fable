set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  func notproc( ; Bool) {
    Bool:true(Unit());
  };

  proc main( ; ; Bool) {
    // notproc is not a proc.
    notproc( ; );
  };
}
fblc-check-error $prg 11:5
