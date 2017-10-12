set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      struct Donut();
      union MultiField(Unit x, Donut y);
      func main( ; MultiField);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
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
