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

      struct Foo(Unit a, Unit b);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      # We haven't imported the Unit type.
      struct Foo(Unit a, Unit b);
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:4:18
