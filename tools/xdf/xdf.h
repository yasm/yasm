/* XDF - Extended Dynamic Object Format */
/* Version 1.4 */
/* $Id$ */

/* FILE HEADER        */
/* SECTION HEADERS [] */
/* SYMBOL TABLE    [] */
/* STRINGS            */
/*                    */
/* SECTION DATA       */
/* SECTION REL     [] */
/*                    */
/* SECTION DATA       */
/* SECTION REL     [] */
/*                    */
/* ...                */

#ifndef XDF_H_INCLUDED
#define XDF_H_INCLUDED

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef enum {

  XDF_MAGIC = 0x87654321

} xdf_magic;


typedef enum {
  
  XDF_SECT_ABSOLUTE = 0x01,  /* Segment has an absolute offset */
  XDF_SECT_FLAT     = 0x02,  /* Data displacements relative to 0, otherwise relative to segment */
  XDF_SECT_BSS      = 0x04,  /* Segment has no data */

  XDF_SECT_USE_16   = 0x10,
  XDF_SECT_USE_32   = 0x20,
  XDF_SECT_USE_64   = 0x40,

} xdf_sect_flags;

typedef enum {

  XDF_SYM_EXTERN = 1,  /* External Symbol */
  XDF_SYM_GLOBAL = 2,  /* Global Symbol   */
  XDF_SYM_EQU    = 4,  /* EQUate Symbol (value, not address) */

} xdf_sym_flags;

typedef enum {

  XDF_RELOC_REL  = 1,  /* DISP = ADDRESS - SEGMENT           (Relative to a segment)             */
  XDF_RELOC_WRT  = 2,  /* DISP = ADDRESS - SYMBOL            (Relative to a symbol)              */
  XDF_RELOC_RIP  = 4,  /* DISP = ADDRESS - RIP - RELOC_SIZE  (Relative to end of an instruction) */
  XDF_RELOC_SEG  = 8,  /* DISP = SEGMENT/SELECTOR OF SYMBOL  (Not symbol itself, but which segment it's in) */

} xdf_rtype;

typedef enum {  

  XDF_RELOC_8  = 1,         
  XDF_RELOC_16 = 2,      
  XDF_RELOC_32 = 4,      
  XDF_RELOC_64 = 8,

} xdf_rsize;


typedef struct {      /* 16 bytes */

  u32 f_magic;        /* magic number                                  */
  u32 f_nsect;        /* number of sections                            */
  u32 f_nsyms;        /* number of symtab entries                      */
  u32 f_size;         /* size of sect headers + symbol table + strings */

} FILE_HEADER;


typedef struct {     /* 32 bytes */

  u32 s_name_idx;    /* section name in symtab          */
  u64 s_addr;        /* physical address                */
  u16 s_align;       /* section alignment (0 ... 4096)  */
  u16 s_flags;       /* flags                           */

  u32 s_data_off;    /* file offset to section data     */
  u32 s_data_size;   /* section data size               */  
  u32 s_reltab_off;  /* file offset to relocation table */
  u32 s_num_reloc;   /* number of relocations entries   */

} SECTION_HEADER;


typedef struct {     /* 16 bytes */

  u32 e_sect_idx;    /* section index to which symbol belongs (-1 = EXTERN) */
  u32 e_sect_off;    /* symbol offset into section data        */
  u32 e_name_off;    /* file offset to null terminated strings */
  u32 e_flags;       /* EXTERN, GLOBAL */

} SYMBOL_ENTRY;


typedef struct {     /* 16 bytes */

  u32 r_off;         /* offset into current section          */
  u32 r_targ_idx;    /* target symbol                        */
  u32 r_base_idx;    /* base symbol if WRT                   */
  u8  r_type;        /* type of relocation (ABS,SEG,RIP,WRT) */
  u8  r_size;        /* size of relocation (1,2,4,8)         */
  u8  r_shift;       /* relocation shift   (0,4,8,16,24,32)  */
  u8  r_flags;       

} RELOCATION_ENTRY;


#endif /* XDF_H_INCLUDED */


