%ifdef foo
	mov	ax, foo
%endif
%ifdef bar
	mov	ax, bar
%endif
%define foo 5
%ifdef foo
	mov	ax, foo
%endif
%ifdef bar
	mov	ax, bar
%endif
