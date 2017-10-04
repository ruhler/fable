set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      # A struct and a process can't have the same name.
      struct Unit();
      struct A(Unit x, Unit y);

      proc A( ; Unit a, Unit b ; Unit) {
        $(Unit());
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:7:12
