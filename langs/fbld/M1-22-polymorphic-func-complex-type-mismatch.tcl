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

    func JustValue<T>(Maybe<T> m; T) {
      m.just;
    };

    func main( ; Bool) {
      # Maybe<Unit> is passed when Maybe<Bool> is expected.
      JustValue<Bool>(Maybe<Unit>:just(Unit()));
    };
  };
}

fbld-check-error $prg 20:23
