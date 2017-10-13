set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct Donut();
      union MultiField(Unit x, Donut y);
      func main( ; MultiField);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct Donut();
      struct MultiField(Unit x, Donut y);   # Should be 'union', not 'struct'.

      func main( ; MultiField) {
        MultiField(Unit(), Donut());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:5:14
