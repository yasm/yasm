%define foo 5
%define bar baz
	mov	ax, [foo+bar]
%define baz bzzt
%define bzzt 9
	mov	ax, baz+bar
