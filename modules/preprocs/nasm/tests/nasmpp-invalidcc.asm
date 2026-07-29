%macro ret_if_cc 1-*
  j%+2    %%skip
  ret
  j%-2    %%skip
  ret
  %%skip:
%endmacro

ret_if_cc ne
