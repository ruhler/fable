set prg {
  MainI.fbld {
    interf MainI {
      # Unit is not declared in the interf, even though it is properly declared
      # in the module.
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg MainM MainI.fbld:5:20
