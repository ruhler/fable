set prg {
  interf MainI {
    # Unit is not declared in the interf, even though it is properly declared
    # in the module.
    func main( ; Unit);
  };

  module MainM(MainI) {
    struct Unit();

    func main( ; Unit) {
      Unit();
    };
  };
}

fbld-check-error $prg 5:18
