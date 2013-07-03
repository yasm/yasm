;
; Test for windbg's ability to single step thru two (or more) source files
; contributing instructions to the same sections.
;
; YASM 1.2.0 and earlier used to generate one CV8_LINE_NUMS per file per
; section, thus potentially having several CV8_LINE_NUMS records for each
; section. It is seems that this confuses either the linker or/and windbg
; and prevents single stepping in and out of include files.
;
; MASM generates one line number debug subsection for each code section,
; repeating the 12 bytes before the offset/lineno pairs for each new file.
;
; It also appears that line numbers must be ordered by section offset, and
; therefore cannot be grouped per file as done by YASM 1.2.0 and earlier.
;
_start:
global _start
%include "cv8-multi.mac"
        mov     eax, 42
%include "cv8-multi.mac"
        xor     eax, eax
        ret

