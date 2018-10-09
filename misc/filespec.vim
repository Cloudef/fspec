" Vim syntax file
" Language: Filespec
" Author: Jari Vetoniemi

syntax clear

syn match   fsBadContinuation contained "\\\s\+$"
syn keyword fsTodo            contained TODO FIXME XXX
syn cluster fsCommentGroup    contains=fsTodo,fsBadContinuation
syn region  fsComment         start="//" skip="\\$" end="$" keepend contains=@fsCommentGroup,@Spell

syn keyword fsKeyword   select until
syn keyword fsStructure enum struct
syn keyword fsConstant  nul dec hex str true false
syn match fsPrimitive   "[su][1-9][0-9]*"

syn case ignore
syn match fsNumbers    display transparent "\<\d\|\.\d" contains=fsNumber,fsFloat,fsOctalError,fsOctal
syn match fsNumbersCom display contained transparent "\<\d\|\.\d" contains=fsNumber,fsFloat,fsOctal
syn match fsNumber     display contained "\d\+\(u\=l\{0,2}\|ll\=u\)\>"
syn match fsNumber     display contained "0x\x\+\(u\=l\{0,2}\|ll\=u\)\>"
syn match fsOctal      display contained "0\o\+\(u\=l\{0,2}\|ll\=u\)\>" contains=fsOctalZero
syn match fsOctalZero  display contained "\<0"
syn match fsFloat      display contained "\d\+f"
syn match fsFloat      display contained "\d\+\.\d*\(e[-+]\=\d\+\)\=[fl]\="
syn match fsFloat      display contained "\.\d\+\(e[-+]\=\d\+\)\=[fl]\=\>"
syn match fsFloat      display contained "\d\+e[-+]\=\d\+[fl]\=\>"
syn match fsOctalError display contained "0\o*[89]\d*"
syn case match

syn match fsSpecial display contained "\\\(x\x\+\|\o\{1,3}\|.\|$\)"
syn match fsString1 "'[^']*'" contains=fsSpecial
syn match fsString2 '"[^"]*"' contains=fsSpecial
syn match fsBinary  "0b[0-1x]\+"

syn match fsBlock    "[{}]"
syn match fsBracket  "[\[\]]"
syn match fsOperator display "[-+&|<>=!*\/~.,;:%&^?()]" contains=fsComment

" Define the default highlighting.
" Only used when an item doesn't have highlighting yet
hi def link fsTodo       Todo
hi def link fsComment    Comment
hi def link fsKeyword    Keyword
hi def link fsStructure  Structure
hi def link fsPrimitive  Type
hi def link fsConstant   Constant
hi def link fsBinary     Number
hi def link fsNumber     Number
hi def link fsOctal      Number
hi def link fsOctalZero  PreProc
hi def link fsFloat      Float
hi def link fsOctalError Error
hi def link fsString1    Character
hi def link fsString2    Character
hi def link fsSpecial    SpecialChar
hi def link fsBlock      Constant
hi def link fsBracket    Constant
hi def link fsOperator   Operator
