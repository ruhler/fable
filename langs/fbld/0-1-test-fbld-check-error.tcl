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

      X X X X X X

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:5:7
