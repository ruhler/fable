set prg {
  # Application cannot be done on a process.
  struct Unit();

  proc myproc( ; Unit x ; Unit) {
    $(x);
  };

  func main( ; Unit) {
    myproc(Unit());
  };
}
fblc-check-error $prg 10:5
