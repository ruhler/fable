set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      struct A(Unit x, Unit y);
      func main( ; A);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      struct A(Unit x, Unit y);

      func main( ; A) {
        Unit v = Unit();
        A(v, v);
      };
    };
  }
}

fbld-test $prg "main@Main" {} {
  return A@Main(Unit@Main(),Unit@Main())
}
