set prg {
  # It's okay to explicitly specify an empty list of static parameters.
  struct Unit<>();

  func main( ; Unit) {
    Unit();
  };
}

fbld-test $prg "main" {} {
  return Unit()
}
