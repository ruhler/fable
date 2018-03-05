set prg {
  struct Unit();
  union Bool(Unit true, Unit false);
  union Maybe<type T>(T just, Unit nothing);

  func main( ; Bool) {
    # There are too many arguments to 'Maybe'
    Maybe<Bool, Unit>:just(Bool:true(Unit())).just;
  };
}

fbld-check-error $prg 8:5
