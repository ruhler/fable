set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();

      func main( ; Unit) {
        Foo:bar(Unit()).bar;    # Union type Foo not declared.
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:6:9
