set prg {
  struct Unit();

  func main( ; Unit) {
    # It's okay to explicitly specify an empty list of static parameter
    # arguments.
    Unit<>();
  };
}

fbld-test $prg "main" {} {
  return Unit()
}
