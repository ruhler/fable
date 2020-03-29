" Vim syntax file
" Language:	Fble
" Maintainer:	Richard Uhler <ruhler@degralder.com>

" TODO: Using \w for normal characters fails to match many characters.
" Is there some way to factor out the actual regex for a normal character?

" Highlight comments as comments.
syn match Comment "#.*"

" Highlight type variables as types.
syn match Type "\w\+@"

" Highlight kinds as special, to distinguish from types.
" But make sure we don't highlight the opening of @(...) as a kind.
syn match xImplicitStructStart "@("
syn match Special "[<@, >]*[@%]" contains=xImplicitStructStart


" Highlight fields used in union select and implicit type struct values as
" labels.
syn match Label "\w*:"
syn match Label "\w\+@:"

" Highlight module paths as include.
syn match Include "\(\w\|[/]\)\+%"

" Highlight literals as strings.
syn match String "|\w\+"
syn region String start=+'+ skip=+''+ end=+'+

