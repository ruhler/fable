fble-test-error 6:5 {
  @ Unit@ = *();

  <<@>@ A@> (A@ a){
    # a<Unit@> is not a struct type, so this should not be allowed.
    a<Unit@>(Unit@());
  };
}
