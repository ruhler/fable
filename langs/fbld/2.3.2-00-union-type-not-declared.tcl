set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      func main( ; Unit) {
        Foo:bar(Unit()).bar;    # Union type Foo not declared.
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:6:9
