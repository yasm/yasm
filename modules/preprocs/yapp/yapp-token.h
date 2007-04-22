typedef union {
    char *str_val;
    struct {
        char *str;
        unsigned long val;
    } int_str_val;
    struct {
        char *str;
        double val;
    } double_str_val;
} YYSTYPE;
#define INTNUM  257
#define FLTNUM  258
#define STRING  259
#define IDENT   260
#define CLEAR   261
#define INCLUDE 262
#define LINE    263
#define DEFINE  264
#define UNDEF   265
#define ASSIGN  266
#define MACRO   267
#define ENDMACRO        268
#define ROTATE  269
#define REP     270
#define EXITREP 271
#define ENDREP  272
#define IF      273
#define ELIF    274
#define ELSE    275
#define ENDIF   276
#define IFDEF   277
#define ELIFDEF 278
#define IFNDEF  279
#define ELIFNDEF        280
#define IFCTX   281
#define ELIFCTX 282
#define IFIDN   283
#define ELIFIDN 284
#define IFIDNI  285
#define ELIFIDNI        286
#define IFID    287
#define ELIFID  288
#define IFNUM   289
#define ELIFNUM 290
#define IFSTR   291
#define ELIFSTR 292
#define ERROR   293
#define PUSH    294
#define POP     295
#define REPL    296
#define LEFT_OP 297
#define RIGHT_OP        298
#define SIGNDIV 299
#define SIGNMOD 300
#define UNARYOP 301
#define WHITESPACE      302


extern YYSTYPE yapp_preproc_lval;
extern char *yapp_preproc_current_file;
extern int yapp_preproc_line_number;

int yapp_preproc_lex(void);
