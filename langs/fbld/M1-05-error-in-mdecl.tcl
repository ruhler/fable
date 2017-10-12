set prg {
  MainI.fbld {
    mtype MainI {
      # Unit is not declared in the mtype, even though it is properly declared
      # in the mdefn.
      func main( ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg MainM Main.mtype:5:20
