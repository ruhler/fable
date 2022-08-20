" Vim syntax file
" Language:	Fble
" Maintainer:	Richard Uhler <ruhler@degralder.com>

" Indent stuff.
" TODO: Move this to a separate indent file.
set comments=:#
set formatoptions=croqtl
set cindent
set cinkeys=0{,0},!^F,o,O
set cinwords=
set cinoptions=(1s

" Use lower and upper character classes for unquoted words, which support
" multibyte characters where alnum does not according to the vim help text.
let s:unquoted = '\([[:lower:][:upper:][:digit:]_]\+\)'
let s:quoted = "'\\(\\([^']\\|''\\)*'\\)"
let s:word = '\(' . s:unquoted . '\|' . s:quoted . '\)'

exec 'syn match fbleComment "#.*"'
exec 'syn match fbleType "' . s:word . '@"'
exec 'syn match fbleLabel "' . s:word . ':"'
exec 'syn match fbleLabel ":"'
exec 'syn match fbleLiteral "|' . s:word . '"'
exec 'syn match fbleModulePath "\(/' . s:word . '\)\+%"'
exec 'syn match fblePoly "[@<>%]"'
exec 'syn match fbleNotPoly "<-"'

" Specify highlight groups to use for each of the syntax groups.
hi def link fbleComment Comment
hi def link fbleLiteral String
hi def link fbleLabel Label
hi def link fbleModulePath Include
hi def link fbleType Type
hi def link fblePoly Special
