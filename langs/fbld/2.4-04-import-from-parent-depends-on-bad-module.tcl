set prg {
  module B {
    # The type Blah is not defined.
    struct Bad(Blah x);
  };

  module A {
    # The import references an entity in a module with errors.
    import @ { Bad@B; };

    func main( ; Bad) {
      Bad();
    };
  };
}

fbld-check-error $prg 4:16
