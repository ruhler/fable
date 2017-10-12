set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      union Bool(Unit True, Unit False);
      func foo(Bool x ; Bool);
      func main( ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      union Bool(Unit True, Unit False);

      func foo(Bool x ; Bool) {
        y;  # The variable 'y' has not been declared.
      };

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:9
