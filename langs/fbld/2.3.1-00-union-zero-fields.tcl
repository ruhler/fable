set prg {
  MainI.fbld {
    interf MainI {
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      union NoFields();   # A union must have at least one field.
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:4:22
