set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      union Recursive(Unit x, Recursive y);  # Self recursive union
      func main( ; Recursive);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      union Recursive(Unit x, Recursive y);

      func main( ; Recursive) {
        Recursive:y(Recursive:x(Unit()));
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Recursive@MainM:y(Recursive@MainM:x(Unit@MainM()))
}
