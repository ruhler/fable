set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      # Two unions with different names are considered different types, even
      # if they have the same fields.
      struct Unit();

      union A(Unit x, Unit y);
      union B(Unit x, Unit y);

      func main( ; A) {
        B:x(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:11:9
