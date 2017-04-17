# Test the most basic 'fbld-test' test.
set mdecl {
  mdecl Main() {
    struct Unit();
    func main( ; Unit);
  };
}

set mdefn {
  mdefn Main() {
    func main( ; Unit) {
      Unit();
    };
  };
}

set prg [list [list Main.mdecl $mdecl] [list Main.mdefn $mdefn]]

fbld-test $prg Main@main {} "return Main@Unit()"
