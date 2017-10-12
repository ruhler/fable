set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      func main( ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      func main( ; Unit) {
        # The type 'Foo' has not been declared.
        Foo v = Unit();
        Unit();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:9
