#define CLOSE 257
#define ID 258
#define CODE 259
#define RANGE 260
#define STRING 261
typedef union {
    Symbol	*symbol;
    RegExp	*regexp;
    Token	*token;
    char	op;
} YYSTYPE;
extern YYSTYPE yylval;
