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
    union Maybe<T>(T just, Unit nothing);
    func main( ; Bool) {
      # The type argument to 'Maybe' is missing
      Maybe:just(Bool:true(Unit())).just;
    };
  };
}

fbld-check-error $prg 15:7
