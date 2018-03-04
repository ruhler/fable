set prg {
  struct Unit();
  union Bool(Unit true, Unit false);
  union Maybe<type T>(T just, Unit nothing);

  func main( ; Bool) {
    Maybe<Bool>:just(Bool:true(Unit())).just;
  };
}

fbld-test $prg "main" {} {
  return Bool:true(Unit())
}
