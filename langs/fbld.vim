" Vim syntax file
" Language:	Fbld
" Maintainer:	Richard Uhler <ruhler@degralder.com>

syn case match
syn keyword	fblcStructure	type struct union func proc
syn keyword fblcInclude mtype mdefn using
"syn region fblcInclude start=+@+ end=+>+
syn match fblcComment "#.*"

hi def link fblcStructure	Structure
hi def link fblcInclude	Include
hi def link fblcComment	Comment
hi def link fblcId Identifier
