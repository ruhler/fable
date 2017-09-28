set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      union Recursive(Unit x, Recursive y);  # Self recursive union
      func main( ; Recursive);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      union Recursive(Unit x, Recursive y);

      func main( ; Recursive) {
        Recursive:y(Recursive:x(Unit()));
      };
    };
  }
}

fbld-test $prg "main@Main<;>" {} {
  return Recursive@Main<;>:y(Recursive@Main<;>:x(Unit@Main<;>()))
}
