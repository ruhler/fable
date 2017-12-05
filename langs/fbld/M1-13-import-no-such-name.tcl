set prg {
  UnitI.fbld {
    interf UnitI {
      struct Unit();
    };
  }

  UnitM.fbld {
    module UnitM(UnitI) {
      struct Unit();
    };
  }

  MainI.fbld {
    interf MainI {
      import @ { UnitM; };
      import UnitM { Unit; };
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      import @ { UnitM; };
      import UnitM { Unit; f; };  # f is not defined
      func main( ; Unit) {
        f();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:4:28
