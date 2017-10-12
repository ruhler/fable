set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      func main( ; Unit);
    };
  }

  MainM.fbld {
    # The mdefn name cannot be the same as the mtype name.
    mdefn MainM(MainI) {
      struct Unit();

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:3:11
