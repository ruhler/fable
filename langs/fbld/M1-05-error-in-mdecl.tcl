set prg {
  Main.mtype {
    mtype Main {
      # Unit is not declared in the mtype, even though it is properly declared
      # in the mdefn.
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg Main Main.mtype:5:20
