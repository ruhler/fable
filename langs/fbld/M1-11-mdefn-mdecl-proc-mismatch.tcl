set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      proc f( ; Unit x, Unit y; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      # The declaration of 'f' here doesn't match the declaration in
      # Main.interf.
      proc f( ; Unit x ; Unit) {
        $(Unit());
      };
    };
  }
}

skip fbld-check-error $prg MainM MainM.fbld:7:12
