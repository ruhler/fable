set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      union Bool(Unit true, Unit false);
      union Maybe<T>(T just, Unit nothing);
      func main( ; Bool);
    };
  }

  MainM.fbld {
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
}

fbld-check-error $prg MainM MainM.fbld:13:25
