" Vim syntax file
" Language:	Fblc
" Maintainer:	Richard Uhler <ruhler@degralder.com>

syn case match
syn keyword	fblcStructure	struct union func proc
syn match fblcComment "#.*"

hi def link fblcStructure	Structure
hi def link fblcComment	Comment
