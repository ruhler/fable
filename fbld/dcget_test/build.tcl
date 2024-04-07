namespace eval "fbld/dcget_test" {
  build $::b/fbld/dcget_test/all.got \
    "$::s/fbld/dcget_test/input.txt $::s/fbld/dcget.tcl" \
    "tclsh8.6 $::s/fbld/dcget.tcl < $::s/fbld/dcget_test/input.txt > $::b/fbld/dcget_test/all.got"
  test $::b/fbld/decget_test/all.tr \
    "$::s/fbld/dcget_test/all.want $::b/fbld/dcget_test/all.got" \
    "diff --strip-trailing-cr $::s/fbld/dcget_test/all.want $::b/fbld/dcget_test/all.got"

  set ids {
    blank_line next_line_file
    next_line_func same_line_file same_line_func
  }
  foreach id $ids {
    build $::b/fbld/dcget_test/$id.got \
      "$::s/fbld/dcget_test/input.txt $::s/fbld/dcget.tcl" \
      "tclsh8.6 $::s/fbld/dcget.tcl $id < $::s/fbld/dcget_test/input.txt > $::b/fbld/dcget_test/$id.got"
    test $::b/fbld/dcget_test/$id.tr \
    "$::s/fbld/dcget_test/$id.want $::b/fbld/dcget_test/$id.got" \
    "diff --strip-trailing-cr $::s/fbld/dcget_test/$id.want $::b/fbld/dcget_test/$id.got"
  }
}
