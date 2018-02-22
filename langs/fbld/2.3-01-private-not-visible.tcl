set prg {
  module StdLib {
    struct Unit();

    priv func mkUnit( ; Unit) {
      Unit();
    };
  };

  func main( ; Unit@StdLib) {
    # mkUnit is private in StdLib, so we are not allowed to access it.
    mkUnit@StdLib();
  };
}

fbld-check-error $prg 12:5
