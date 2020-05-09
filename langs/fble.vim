" Vim syntax file
" Language:	Fble
" Maintainer:	Richard Uhler <ruhler@degralder.com>

" Indent stuff.
set comments=:#
set formatoptions=croqtl
set cindent
set cinkeys=0{,0},!^F,o,O
set cinwords=
set cinoptions=(1s

" TODO: Using \w for normal characters fails to match many characters.
" Is there some way to factor out the actual regex for a normal character?

" Highlight comments as comments.
syn match Comment "#.*"

" Highlight type variables as types.
syn match Type "\w\+@"

" Highlight kinds as special, to distinguish from types.
syn match xNotKind "@("
syn match xNotKind "@<"
syn match Special "\(<[<@, >]*>\)\?[@%]" contains=xNotKind


" Highlight fields used in union select and implicit type struct values as
" labels.
syn match Label "\w*:"
syn match Label "\w\+@:"

" Highlight module paths as include.
syn match Include "\(\w\|[/]\)\+%"

" Highlight literals as strings.
syn match String "|\w\+"
syn region String start=+'+ skip=+''+ end=+'+

