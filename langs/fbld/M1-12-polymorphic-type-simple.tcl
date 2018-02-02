set prg {
  interf MainI {
    struct Unit();
    union Bool(Unit true, Unit false);
    union Maybe<type T>(T just, Unit nothing);
    func main( ; Bool);
  };

  module MainM(MainI) {
    struct Unit();
    union Bool(Unit true, Unit false);
    union Maybe<type T>(T just, Unit nothing);
    func main( ; Bool) {
      Maybe<Bool>:just(Bool:true(Unit())).just;
    };
  };
}

skip fbld-test $prg "main@MainM" {} {
  return Bool@MainM:true(Unit@MainM())
}
