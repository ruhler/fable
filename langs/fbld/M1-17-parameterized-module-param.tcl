set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      interf Make<T> {
        func make( ; T);
      };

      # Test that we can declare and use a parameterized module parameter.
      func foo<A ; Make<A> M>( ; A) {
        make@M();
      };

      module MakeUnit(Make<Unit>) {
        import @ { Unit; };

        func make( ; Unit) {
          Unit();
        };
      };

      func main( ; Unit) {
        foo<Unit ; MakeUnit>();
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Unit@MainM()
}
