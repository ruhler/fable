set prg {
  MainI.fbld {
    interf MainI {
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      # Declared functions must have a name.
      struct Unit();

      func (Unit x, Unit y; Unit) {
        x;
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:6:12
