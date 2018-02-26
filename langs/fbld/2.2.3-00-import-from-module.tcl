set prg {
  # Importing from module makes the Unit type visible in the parent.
  import M { Unit; };

  module M {
    struct Unit();
  };

  func main( ; Unit) {
    Unit();
  };
}

fbld-test $prg "main" {} {
  return Unit@M()
}
