set prg {
  struct Unit();

  # Strange characters such as the following are not allowed.
  π π π π π π π 

  func main( ; Unit) {
    Unit();
  };
}

fblc-check-error $prg 5:3
