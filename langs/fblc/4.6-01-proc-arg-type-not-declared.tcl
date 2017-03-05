set prg {
  struct Unit();

  # The process argument type Donut is not defined.
  proc f( ; Unit x, Donut y; Unit) {
    $(x);
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 5:21
