%define foo(bar) bar+1
%define foo(bar, baz) bar-baz
	mov	ax, foo
	mov	ax, foo(5)
	mov	ax, foo(5, 3)
	mov	ax, foo(5, 6, 7)
