if exists("b:current_syntax")
  finish
endif

syn match cirnoInteger '\d\+'
syn match cirnoInteger '[-+]\d\+'

syn region cirnoString start='"' end='"'

syn keyword cirnoFunction fn
syn keyword cirnoStatement if while return break else asm
syn keyword cirnoType i8 i32 struct

hi def link cirnoFunction Function
hi def link cirnoStatement Statement
hi def link cirnoType Type
hi def link cirnoInteger Constant
hi def link cirnoString Constant

let b:current_syntax = '9c'
