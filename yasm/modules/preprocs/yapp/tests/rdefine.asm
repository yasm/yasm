%define recursion endless
%define endless recursion
	mov	ax, 5
	mov	ax, endless
%define recurse recurse
	mov	ax, recurse
