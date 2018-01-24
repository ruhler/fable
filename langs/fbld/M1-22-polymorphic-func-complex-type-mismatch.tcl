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

    func JustValue<type T>(Maybe<T> m; T) {
      m.just;
    };

    func main( ; Bool) {
      # Maybe<Unit> is passed when Maybe<Bool> is expected.
      JustValue<Bool>(Maybe<Unit>:just(Unit()));
    };
  };
}

skip fbld-check-error $prg 20:23
