" Vim syntax file
" Language:	Fbld
" Maintainer:	Richard Uhler <ruhler@degralder.com>

if exists("b:current_syntax")
  finish
endif

syn match fbldBracket "@"
syn match fbldTag "@[a-zA-Z0-9_]\+"

syn match fbldBracket "\["
syn match fbldBracket "\]"

syn region fbldContainedNormal start="{" end="}" contains=fbldContainedNormal contained
syn region fbldNormal matchgroup=fbldBracket start="{" end="}" contains=fbldContainedNormal

syn match fbldEscape "\\\]"
syn match fbldEscape "\\\["
syn match fbldEscape "\\@"
syn match fbldEscape "\\\\"
syn match fbldEscape "\\n"

" Specify highlight groups to use for each of the syntax groups.
hi def link fbldTag Identifier
hi def link fbldEscape Character
hi def link fbldBracket Special
hi def link fbldNormal Normal
hi def link fbldContainedNormal Normal

let b:current_syntax = "fbld"
