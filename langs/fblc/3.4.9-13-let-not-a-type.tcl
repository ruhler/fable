set prg {
  struct Unit();
   
  func Foo( ; Unit) {
    Unit();
  };

  func main( ; Unit) {
    # 'Foo' should refer to a type, but it refers to a function.
    Foo v = Unit();
    Unit();
  };
}
fblc-check-error $prg 10:5
