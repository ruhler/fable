set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      proc f( ; Unit x, Unit y; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      # The declaration of 'f' here doesn't match the declaration in
      # Main.mtype.
      proc f( ; Unit x ; Unit) {
        $(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:12
