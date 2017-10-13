set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      func main( ; Unit) {
        # Missing a semicolon
        Unit()
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:8:7
