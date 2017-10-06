set prg {
  Main.mtype {
    mtype Main {
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      union NoFields();   # A union must have at least one field.
    };
  }
}

fbld-check-error $prg Main Main.mdefn:4:22
