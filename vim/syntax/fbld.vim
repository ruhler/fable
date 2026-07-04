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

" Highlighting of inline literal args.
"   The fbldContainedNormal is for nested {} within inline args.
"   The "}{" start is for inline literal arg following an inline literal arg.
"   The "]{" start is for inline literal arg following an inline normal arg.
"   The "@...{" start is for inline args following a tag or continuation.
" The goal is to avoid matching things like "@l[{]@hi@l[}]" as inline literal
" args.
syn region fbldContainedNormal start="{" end="}" contains=fbldContainedNormal contained
syn region fbldNormal matchgroup=fbldBracket start="}\@<={" end="}" contains=fbldContainedNormal
syn region fbldNormal matchgroup=fbldBracket start="]\@<={" end="}" contains=fbldContainedNormal
syn region fbldNormal matchgroup=fbldBracket start="\(@[a-zA-Z0-9_]*\)\@<={" end="}" contains=fbldContainedNormal

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
