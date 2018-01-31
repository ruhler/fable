set prg {
  interf MainI {
    struct Unit();
    union Bool(Unit true, Unit false);
    union Maybe<type T>(T just, Unit nothing);
    func main( ; Bool);
  };

  module MainM(MainI) {
    struct Unit();
    union Bool(Unit true, Unit false);
    union Maybe<type T>(T just, Unit nothing);
    func main( ; Bool) {
      # The Maybe constructor is passed a Unit, not a Bool.
      Maybe<Bool>:just(Unit()).just;
    };
  };
}

fbld-check-error $prg 15:24
