" Vim syntax file
" Language: ufoai scipt
" Maintainer: Igor Gritsenko
" Latest Revision: 08 August 2014

if exists("b:current_syntax")
  finish
endif

syn keyword UFOTodo containedin=UFOComment contained TODO FIXME XXX NOTE

syn keyword UFOKeywords component model bar string window battlescape panel image radiobutton confunc button container data text func

syn keyword UFOBoolean true false

syn match UFOComment "//.*$" contains=UFOTodo
syn region UFOComment  start="/\*"  end="\*/" contains=UFOTodo
syn region UFOComment start="/\*\*" end="\*/" contains=UFOTodo
syn region UFOBlock start="{" end="}" fold transparent

syn region UFOString start="\"" end="\""

syn match UFONumber '\d\+'
syn match UFONumber '[-+]\d+'

syn match UFONumber '\d\+\.\d*'
syn match UFONumber '[-+]\d\+\.\d*'


let b:current_syntax = "ufo"

hi def link UFOTodo         Todo
hi def link UFOComment      Comment
hi def link UFOBlock  		PreProc
hi def link UFOString		String
hi def link UFONumber		Number
hi def link UFOKeywords		Keyword
hi def link UFOBoolean		Boolean
