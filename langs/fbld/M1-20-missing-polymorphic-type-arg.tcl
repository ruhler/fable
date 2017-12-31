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
      func main( ; Bool) {
        # The type argument to 'Maybe' is missing
        Maybe:just(Bool:true(Unit())).just;
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:9
