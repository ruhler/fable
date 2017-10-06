set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      union OneField(Unit x);
      func main( ; OneField);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      union OneField(Unit x);

      func main( ; OneField) {
        OneField:x(Unit());
      };
    };
  }
}

fbld-test $prg "main@Main" {} {
  return OneField@Main:x(Unit@Main())
}
