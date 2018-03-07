set prg {
  priv struct Unit();

  # The public entity main must not refer to private entity Unit in the
  # prototype.
  func main( ; Unit) {
    Unit();
  };
}

fbld-check-error $prg 6:16
