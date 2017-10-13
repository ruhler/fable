set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
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
