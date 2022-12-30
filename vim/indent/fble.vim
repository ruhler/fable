" Vim indent file
" Language:	Fble
" Maintainer:	Richard Uhler <ruhler@degralder.com>

if exists("b:did_indent")
  finish
endif
let b:did_indent = 1

setlocal cindent
setlocal cinkeys=0{,0},!^F,o,O
setlocal cinwords=
setlocal cinoptions=(1s
