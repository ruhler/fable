set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      struct Donut();
      struct BadStruct(Unit x, Donut x);  # duplicate fields named 'x'
      func main( ; BadStruct);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      struct Donut();
      struct BadStruct(Unit x, Donut x);

      func main( ; BadStruct) {
        BadStruct(Unit(), Donut());
      };
    };
  }
}

fbld-check-error $prg Main Main.mtype:5:38


# Even if the types are the same.
set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      struct BadStruct(Unit x, Unit x);  # duplicate fields named 'x'
      func main( ; BadStruct);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      struct BadStruct(Unit x, Unit x);

      func main( ; BadStruct) {
        BadStruct(Unit(), Unit());
      };
    };
  }
}

fbld-check-error $prg Main Main.mtype:4:37
