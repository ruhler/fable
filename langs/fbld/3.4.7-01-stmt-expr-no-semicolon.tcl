set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      func main( ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      func main( ; Unit) {
        # Missing a semicolon
        Unit()
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:8:7
