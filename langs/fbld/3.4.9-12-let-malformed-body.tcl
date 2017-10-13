set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct A(Unit x, Unit y);
      func main( ; A);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct A(Unit x, Unit y);

      func main( ; A) {
        # The body of the let is malformed.
        Unit v = Unit();
        ???;
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:9:10
