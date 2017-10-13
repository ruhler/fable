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
        # The function 'f' has not been declared.
        f(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:9
