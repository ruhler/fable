" Vim syntax file
" Language:	Fble
" Maintainer:	Richard Uhler <ruhler@degralder.com>

syn match fbleComment "#.*"
syn match fbleType "[a-zA-Z0-9_]\+@"

hi def link fbleComment	Comment
hi def link fbleType	Type
