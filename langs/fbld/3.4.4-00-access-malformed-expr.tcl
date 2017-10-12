set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      struct A(Unit x, Unit y);
      func main( ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      struct A(Unit x, Unit y);

      func main( ; Unit) {
        # The variable 'x' has not been declared.
        x.y;
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:8:9
