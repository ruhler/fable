set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      union Bool(Unit true, Unit false);
      struct Maybe<T>(T just, Unit nothing);
      func main( ; Bool);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      union Bool(Unit true, Unit false);
      struct Maybe<T>(T just, Unit nothing);
      func main( ; Bool) {
        Maybe<Bool>:just(Bool:true(Unit())).just;
      };
    };
  }
}

fbld-test $prg "main@Main" {} {
  return Bool@Main:true(Unit@Main())
}
