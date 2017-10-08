set prg {
  # The return type of a process must be declared.
  struct Unit();

  proc p(Unit- px, Unit+ py ; Unit x, Unit y ; Donut) {
    $(Donut());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 5:48
