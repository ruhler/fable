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

      # The declaration of 'A' here doesn't match the declaration in
      # Main.interf.
      struct A(Unit x);

      func main( ; A) {
        A(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:14
