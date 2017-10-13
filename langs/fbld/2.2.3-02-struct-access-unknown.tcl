set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      # Accessing an unknown field of a struct.
      struct Unit();
      struct A(Unit x, Unit y);

      func main( ; Unit) {
        A(Unit(), Unit()).z;
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:27
