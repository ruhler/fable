set prg {
  MainI.fbld {
    interf MainI {
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      func main( ; Unit) {
        Foo:bar(Unit()).bar;    # Union type Foo not declared.
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:6:9
