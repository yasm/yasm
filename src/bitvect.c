#ifndef MODULE_BIT_VECTOR
#define MODULE_BIT_VECTOR
/*****************************************************************************/
/*  MODULE NAME:  BitVector.c                           MODULE TYPE:  (adt)  */
/*****************************************************************************/
/*  MODULE IMPORTS:                                                          */
/*****************************************************************************/
#include <stdlib.h>                                 /*  MODULE TYPE:  (sys)  */
#include <limits.h>                                 /*  MODULE TYPE:  (sys)  */
#include <string.h>                                 /*  MODULE TYPE:  (sys)  */
#include <ctype.h>                                  /*  MODULE TYPE:  (sys)  */
#include "ToolBox.h"                                /*  MODULE TYPE:  (dat)  */
/*****************************************************************************/
/*  MODULE INTERFACE:                                                        */
/*****************************************************************************/

typedef enum
    {
        ErrCode_Ok = 0,   /* everything went allright                        */

        ErrCode_Type,     /* types word and size_t have incompatible sizes   */
        ErrCode_Bits,     /* bits of word and sizeof(word) are inconsistent  */
        ErrCode_Word,     /* size of word is less than 16 bits               */
        ErrCode_Long,     /* size of word is greater than size of long       */
        ErrCode_Powr,     /* number of bits of word is not a power of two    */
        ErrCode_Loga,     /* error in calculation of logarithm               */

        ErrCode_Null,     /* unable to allocate memory                       */

        ErrCode_Indx,     /* index out of range                              */
        ErrCode_Ordr,     /* minimum > maximum index                         */
        ErrCode_Size,     /* bit vector size mismatch                        */
        ErrCode_Pars,     /* input string syntax error                       */
        ErrCode_Ovfl,     /* numeric overflow error                          */
        ErrCode_Same,     /* operands must be distinct                       */
        ErrCode_Expo,     /* exponent must be positive                       */
        ErrCode_Zero      /* division by zero error                          */
    } ErrCode;

/* ===> MISCELLANEOUS: <=== */

ErrCode BitVector_Boot       (void);                 /* 0 = ok, 1..7 = error */

N_word  BitVector_Size  (N_int bits);       /* bit vector size (# of words)  */
N_word  BitVector_Mask  (N_int bits);       /* bit vector mask (unused bits) */

/* ===> CLASS METHODS: <=== */

charptr BitVector_Version    (void);               /* returns version string */

N_int   BitVector_Word_Bits  (void);    /* returns # of bits in machine word */
N_int   BitVector_Long_Bits  (void);   /* returns # of bits in unsigned long */

wordptr BitVector_Create(N_int bits, boolean clear);              /* malloc  */

/* ===> OBJECT METHODS: <=== */

wordptr BitVector_Shadow  (wordptr addr);  /* makes new, same size but empty */
wordptr BitVector_Clone   (wordptr addr);           /* makes exact duplicate */

wordptr BitVector_Concat  (wordptr X, wordptr Y);   /* returns concatenation */

wordptr BitVector_Resize  (wordptr oldaddr, N_int bits);          /* realloc */
void    BitVector_Destroy (wordptr addr);                         /* free    */

/* ===> bit vector copy function: */

void    BitVector_Copy    (wordptr X, wordptr Y);           /* X = Y         */

/* ===> bit vector initialization: */

void    BitVector_Empty   (wordptr addr);                   /* X = {}        */
void    BitVector_Fill    (wordptr addr);                   /* X = ~{}       */
void    BitVector_Flip    (wordptr addr);                   /* X = ~X        */

void    BitVector_Primes  (wordptr addr);

/* ===> miscellaneous functions: */

void    BitVector_Reverse (wordptr X, wordptr Y);

/* ===> bit vector interval operations and functions: */

void    BitVector_Interval_Empty   (wordptr addr, N_int lower, N_int upper);
void    BitVector_Interval_Fill    (wordptr addr, N_int lower, N_int upper);
void    BitVector_Interval_Flip    (wordptr addr, N_int lower, N_int upper);
void    BitVector_Interval_Reverse (wordptr addr, N_int lower, N_int upper);

boolean BitVector_interval_scan_inc(wordptr addr, N_int start,
                                    N_intptr min, N_intptr max);
boolean BitVector_interval_scan_dec(wordptr addr, N_int start,
                                    N_intptr min, N_intptr max);

void    BitVector_Interval_Copy    (wordptr X, wordptr Y, N_int Xoffset,
                                    N_int Yoffset, N_int length);

wordptr BitVector_Interval_Substitute(wordptr X, wordptr Y,
                                    N_int Xoffset, N_int Xlength,
                                    N_int Yoffset, N_int Ylength);

/* ===> bit vector test functions: */

boolean BitVector_is_empty         (wordptr addr);          /* X == {} ?     */
boolean BitVector_is_full          (wordptr addr);          /* X == ~{} ?    */

boolean BitVector_equal            (wordptr X, wordptr Y);  /* X == Y ?      */
Z_int   BitVector_Lexicompare      (wordptr X, wordptr Y);  /* X <,=,> Y ?   */
Z_int   BitVector_Compare          (wordptr X, wordptr Y);  /* X <,=,> Y ?   */

/* ===> bit vector string conversion functions: */

charptr BitVector_to_Hex  (wordptr addr);
ErrCode BitVector_from_Hex(wordptr addr, charptr string);

charptr BitVector_to_Bin  (wordptr addr);
ErrCode BitVector_from_Bin(wordptr addr, charptr string);

charptr BitVector_to_Dec  (wordptr addr);
ErrCode BitVector_from_Dec(wordptr addr, charptr string);

charptr BitVector_to_Enum (wordptr addr);
ErrCode BitVector_from_Enum(wordptr addr, charptr string);

void    BitVector_Dispose (charptr string);

/* ===> bit vector bit operations, functions & tests: */

void    BitVector_Bit_Off (wordptr addr, N_int index);      /* X = X \ {x}   */
void    BitVector_Bit_On  (wordptr addr, N_int index);      /* X = X + {x}   */
boolean BitVector_bit_flip(wordptr addr, N_int index);  /* X=(X+{x})\(X*{x}) */

boolean BitVector_bit_test(wordptr addr, N_int index);      /* {x} in X ?    */

void    BitVector_Bit_Copy(wordptr addr, N_int index, boolean bit);

/* ===> bit vector bit shift & rotate functions: */

void    BitVector_LSB         (wordptr addr, boolean bit);
void    BitVector_MSB         (wordptr addr, boolean bit);
boolean BitVector_lsb         (wordptr addr);
boolean BitVector_msb         (wordptr addr);
boolean BitVector_rotate_left (wordptr addr);
boolean BitVector_rotate_right(wordptr addr);
boolean BitVector_shift_left  (wordptr addr, boolean carry_in);
boolean BitVector_shift_right (wordptr addr, boolean carry_in);
void    BitVector_Move_Left   (wordptr addr, N_int bits);
void    BitVector_Move_Right  (wordptr addr, N_int bits);

/* ===> bit vector insert/delete bits: */

void    BitVector_Insert      (wordptr addr, N_int offset, N_int count,
                               boolean clear);
void    BitVector_Delete      (wordptr addr, N_int offset, N_int count,
                               boolean clear);

/* ===> bit vector arithmetic: */

boolean BitVector_increment   (wordptr addr);               /* X++           */
boolean BitVector_decrement   (wordptr addr);               /* X--           */

boolean BitVector_add     (wordptr X, wordptr Y, wordptr Z, boolean carry);
boolean BitVector_subtract(wordptr X, wordptr Y, wordptr Z, boolean carry);
void    BitVector_Negate  (wordptr X, wordptr Y);
void    BitVector_Absolute(wordptr X, wordptr Y);
Z_int   BitVector_Sign    (wordptr addr);
ErrCode BitVector_Mul_Pos (wordptr X, wordptr Y, wordptr Z);
ErrCode BitVector_Multiply(wordptr X, wordptr Y, wordptr Z);
ErrCode BitVector_Div_Pos (wordptr Q, wordptr X, wordptr Y, wordptr R);
ErrCode BitVector_Divide  (wordptr Q, wordptr X, wordptr Y, wordptr R);
ErrCode BitVector_GCD     (wordptr X, wordptr Y, wordptr Z);
ErrCode BitVector_Power   (wordptr X, wordptr Y, wordptr Z);

/* ===> direct memory access functions: */

void    BitVector_Block_Store (wordptr addr, charptr buffer, N_int length);
charptr BitVector_Block_Read  (wordptr addr, N_intptr length);

/* ===> word array functions: */

void    BitVector_Word_Store  (wordptr addr, N_int offset, N_int value);
N_int   BitVector_Word_Read   (wordptr addr, N_int offset);

void    BitVector_Word_Insert (wordptr addr, N_int offset, N_int count,
                               boolean clear);
void    BitVector_Word_Delete (wordptr addr, N_int offset, N_int count,
                               boolean clear);

/* ===> arbitrary size chunk functions: */

void    BitVector_Chunk_Store (wordptr addr, N_int chunksize,
                               N_int offset, N_long value);
N_long  BitVector_Chunk_Read  (wordptr addr, N_int chunksize,
                               N_int offset);

/* ===> set operations: */

void    Set_Union       (wordptr X, wordptr Y, wordptr Z);  /* X = Y + Z     */
void    Set_Intersection(wordptr X, wordptr Y, wordptr Z);  /* X = Y * Z     */
void    Set_Difference  (wordptr X, wordptr Y, wordptr Z);  /* X = Y \ Z     */
void    Set_ExclusiveOr (wordptr X, wordptr Y, wordptr Z);  /* X=(Y+Z)\(Y*Z) */
void    Set_Complement  (wordptr X, wordptr Y);             /* X = ~Y        */

/* ===> set functions: */

boolean Set_subset      (wordptr X, wordptr Y);             /* X subset Y ?  */

N_int   Set_Norm        (wordptr addr);                     /* = | X |       */
Z_long  Set_Min         (wordptr addr);                     /* = min(X)      */
Z_long  Set_Max         (wordptr addr);                     /* = max(X)      */

/* ===> matrix-of-booleans operations: */

void    Matrix_Multiplication(wordptr X, N_int rowsX, N_int colsX,
                              wordptr Y, N_int rowsY, N_int colsY,
                              wordptr Z, N_int rowsZ, N_int colsZ);

void    Matrix_Product       (wordptr X, N_int rowsX, N_int colsX,
                              wordptr Y, N_int rowsY, N_int colsY,
                              wordptr Z, N_int rowsZ, N_int colsZ);

void    Matrix_Closure       (wordptr addr, N_int rows, N_int cols);

void    Matrix_Transpose     (wordptr X, N_int rowsX, N_int colsX,
                              wordptr Y, N_int rowsY, N_int colsY);

/*****************************************************************************/
/*  MODULE RESOURCES:                                                        */
/*****************************************************************************/

#define bits_(BitVector) *(BitVector-3)
#define size_(BitVector) *(BitVector-2)
#define mask_(BitVector) *(BitVector-1)

/*****************************************************************************/
/*  MODULE IMPLEMENTATION:                                                   */
/*****************************************************************************/

    /**********************************************/
    /* global implementation-intrinsic constants: */
    /**********************************************/

#define BIT_VECTOR_HIDDEN_WORDS 3

    /*****************************************************************/
    /* global machine-dependent constants (set by "BitVector_Boot"): */
    /*****************************************************************/

static N_word BITS;     /* = # of bits in machine word (must be power of 2)  */
static N_word MODMASK;  /* = BITS - 1 (mask for calculating modulo BITS)     */
static N_word LOGBITS;  /* = ld(BITS) (logarithmus dualis)                   */
static N_word FACTOR;   /* = ld(BITS / 8) (ld of # of bytes)                 */

static N_word LSB = 1;  /* = mask for least significant bit                  */
static N_word MSB;      /* = mask for most significant bit                   */

static N_word LONGBITS; /* = # of bits in unsigned long                      */

static N_word LOG10;    /* = logarithm to base 10 of BITS - 1                */
static N_word EXP10;    /* = largest possible power of 10 in signed int      */

    /********************************************************************/
    /* global bit mask table for fast access (set by "BitVector_Boot"): */
    /********************************************************************/

static wordptr BITMASKTAB;

    /*****************************/
    /* global macro definitions: */
    /*****************************/

#define BIT_VECTOR_ZERO_WORDS(target,count) \
    while (count-- > 0) *target++ = 0;

#define BIT_VECTOR_FILL_WORDS(target,fill,count) \
    while (count-- > 0) *target++ = fill;

#define BIT_VECTOR_FLIP_WORDS(target,flip,count) \
    while (count-- > 0) *target++ ^= flip;

#define BIT_VECTOR_COPY_WORDS(target,source,count) \
    while (count-- > 0) *target++ = *source++;

#define BIT_VECTOR_BACK_WORDS(target,source,count) \
    { target += count; source += count; while (count-- > 0) *--target = *--source; }

#define BIT_VECTOR_CLR_BIT(address,index) \
    *(address+(index>>LOGBITS)) &= NOT BITMASKTAB[index AND MODMASK];

#define BIT_VECTOR_SET_BIT(address,index) \
    *(address+(index>>LOGBITS)) |= BITMASKTAB[index AND MODMASK];

#define BIT_VECTOR_TST_BIT(address,index) \
    ((*(address+(index>>LOGBITS)) AND BITMASKTAB[index AND MODMASK]) != 0)

#define BIT_VECTOR_FLP_BIT(address,index,mask) \
    (mask = BITMASKTAB[index AND MODMASK]), \
    (((*(addr+(index>>LOGBITS)) ^= mask) AND mask) != 0)

#define BIT_VECTOR_DIGITIZE(type,value,digit) \
    value = (type) ((digit = value) / 10); \
    digit -= value * 10; \
    digit += (type) '0';

    /*********************************************************/
    /* private low-level functions (potentially dangerous!): */
    /*********************************************************/

static N_word power10(N_word x)
{
    N_word y = 1;

    while (x-- > 0) y *= 10;
    return(y);
}

static void BIT_VECTOR_zro_words(wordptr addr, N_word count)
{
    BIT_VECTOR_ZERO_WORDS(addr,count)
}

static void BIT_VECTOR_cpy_words(wordptr target, wordptr source, N_word count)
{
    BIT_VECTOR_COPY_WORDS(target,source,count)
}

static void BIT_VECTOR_mov_words(wordptr target, wordptr source, N_word count)
{
    if (target != source)
    {
        if (target < source) BIT_VECTOR_COPY_WORDS(target,source,count)
        else                 BIT_VECTOR_BACK_WORDS(target,source,count)
    }
}

static void BIT_VECTOR_ins_words(wordptr addr, N_word total, N_word count,
                                 boolean clear)
{
    N_word length;

    if ((total > 0) and (count > 0))
    {
        if (count > total) count = total;
        length = total - count;
        if (length > 0) BIT_VECTOR_mov_words(addr+count,addr,length);
        if (clear)      BIT_VECTOR_zro_words(addr,count);
    }
}

static void BIT_VECTOR_del_words(wordptr addr, N_word total, N_word count,
                                 boolean clear)
{
    N_word length;

    if ((total > 0) and (count > 0))
    {
        if (count > total) count = total;
        length = total - count;
        if (length > 0) BIT_VECTOR_mov_words(addr,addr+count,length);
        if (clear)      BIT_VECTOR_zro_words(addr+length,count);
    }
}

static void BIT_VECTOR_reverse(charptr string, N_word length)
{
    charptr last;
    N_char  temp;

    if (length > 1)
    {
        last = string + length - 1;
        while (string < last)
        {
            temp = *string;
            *string = *last;
            *last = temp;
            string++;
            last--;
        }
    }
}

static N_word BIT_VECTOR_int2str(charptr string, N_word value)
{
    N_word  length;
    N_word  digit;
    charptr work;

    work = string;
    if (value > 0)
    {
        length = 0;
        while (value > 0)
        {
            BIT_VECTOR_DIGITIZE(N_word,value,digit)
            *work++ = (N_char) digit;
            length++;
        }
        BIT_VECTOR_reverse(string,length);
    }
    else
    {
        length = 1;
        *work++ = (N_char) '0';
    }
    return(length);
}

static N_word BIT_VECTOR_str2int(charptr string, N_word *value)
{
    N_word  length;
    N_word  digit;

    *value = 0;
    length = 0;
    digit = (N_word) *string++;
    /* separate because isdigit() is likely a macro! */
    while (isdigit(digit) != 0)
    {
        length++;
        digit -= (N_word) '0';
        if (*value) *value *= 10;
        *value += digit;
        digit = (N_word) *string++;
    }
    return(length);
}

    /*****************************************/
    /* automatic self-configuration routine: */
    /*****************************************/

    /*******************************************************/
    /*                                                     */
    /*   MUST be called once prior to any other function   */
    /*   to initialize the machine dependent constants     */
    /*   of this package! (But call only ONCE!)            */
    /*                                                     */
    /*******************************************************/

ErrCode BitVector_Boot(void)
{
    N_long longsample = 1L;
    N_word sample = LSB;
    N_word lsb;

    if (sizeof(N_word) > sizeof(size_t)) return(ErrCode_Type);

    BITS = 1;
    while (sample <<= 1) BITS++;    /* determine # of bits in a machine word */

    if (BITS != (sizeof(N_word) << 3)) return(ErrCode_Bits);

    if (BITS < 16) return(ErrCode_Word);

    LONGBITS = 1;
    while (longsample <<= 1) LONGBITS++;  /* = # of bits in an unsigned long */

    if (BITS > LONGBITS) return(ErrCode_Long);

    LOGBITS = 0;
    sample = BITS;
    lsb = (sample AND LSB);
    while ((sample >>= 1) and (not lsb))
    {
        LOGBITS++;
        lsb = (sample AND LSB);
    }

    if (sample) return(ErrCode_Powr);      /* # of bits is not a power of 2! */

    if (BITS != (LSB << LOGBITS)) return(ErrCode_Loga);

    MODMASK = BITS - 1;
    FACTOR = LOGBITS - 3;  /* ld(BITS / 8) = ld(BITS) - ld(8) = ld(BITS) - 3 */
    MSB = (LSB << MODMASK);

    BITMASKTAB = (wordptr) malloc((size_t) (BITS << FACTOR));

    if (BITMASKTAB == NULL) return(ErrCode_Null);

    for ( sample = 0; sample < BITS; sample++ )
    {
        BITMASKTAB[sample] = (LSB << sample);
    }

    LOG10 = (N_word) (MODMASK * 0.30103); /* = (BITS - 1) * ( ln 2 / ln 10 ) */
    EXP10 = power10(LOG10);

    return(ErrCode_Ok);
}

N_word BitVector_Size(N_int bits)           /* bit vector size (# of words)  */
{
    N_word size;

    size = bits >> LOGBITS;
    if (bits AND MODMASK) size++;
    return(size);
}

N_word BitVector_Mask(N_int bits)           /* bit vector mask (unused bits) */
{
    N_word mask;

    mask = bits AND MODMASK;
    if (mask) mask = (N_word) ~(~0L << mask); else mask = (N_word) ~0L;
    return(mask);
}

charptr BitVector_Version(void)
{
    return((charptr)"5.8");
}

N_int BitVector_Word_Bits(void)
{
    return(BITS);
}

N_int BitVector_Long_Bits(void)
{
    return(LONGBITS);
}

wordptr BitVector_Create(N_int bits, boolean clear)         /* malloc        */
{
    N_word  size;
    N_word  mask;
    N_word  bytes;
    wordptr addr;
    wordptr zero;

    size = BitVector_Size(bits);
    mask = BitVector_Mask(bits);
    bytes = (size + BIT_VECTOR_HIDDEN_WORDS) << FACTOR;
    addr = (wordptr) malloc((size_t) bytes);
    if (addr != NULL)
    {
        *addr++ = bits;
        *addr++ = size;
        *addr++ = mask;
        if (clear)
        {
            zero = addr;
            BIT_VECTOR_ZERO_WORDS(zero,size)
        }
    }
    return(addr);
}

wordptr BitVector_Shadow(wordptr addr)     /* makes new, same size but empty */
{
    return( BitVector_Create(bits_(addr),true) );
}

wordptr BitVector_Clone(wordptr addr)               /* makes exact duplicate */
{
    N_word  bits;
    wordptr twin;

    bits = bits_(addr);
    twin = BitVector_Create(bits,false);
    if ((twin != NULL) and (bits > 0))
        BIT_VECTOR_cpy_words(twin,addr,size_(addr));
    return(twin);
}

wordptr BitVector_Concat(wordptr X, wordptr Y)      /* returns concatenation */
{
    /* BEWARE that X = most significant part, Y = least significant part! */

    N_word  bitsX;
    N_word  bitsY;
    N_word  bitsZ;
    wordptr Z;

    bitsX = bits_(X);
    bitsY = bits_(Y);
    bitsZ = bitsX + bitsY;
    Z = BitVector_Create(bitsZ,false);
    if ((Z != NULL) and (bitsZ > 0))
    {
        BIT_VECTOR_cpy_words(Z,Y,size_(Y));
        BitVector_Interval_Copy(Z,X,bitsY,0,bitsX);
        *(Z+size_(Z)-1) &= mask_(Z);
    }
    return(Z);
}

wordptr BitVector_Resize(wordptr oldaddr, N_int bits)       /* realloc       */
{
    N_word  bytes;
    N_word  oldsize;
    N_word  oldmask;
    N_word  newsize;
    N_word  newmask;
    wordptr newaddr;
    wordptr source;
    wordptr target;

    oldsize = size_(oldaddr);
    oldmask = mask_(oldaddr);
    newsize = BitVector_Size(bits);
    newmask = BitVector_Mask(bits);
    if (oldsize > 0) *(oldaddr+oldsize-1) &= oldmask;
    if (newsize <= oldsize)
    {
        newaddr = oldaddr;
        bits_(newaddr) = bits;
        size_(newaddr) = newsize;
        mask_(newaddr) = newmask;
        if (newsize > 0) *(newaddr+newsize-1) &= newmask;
    }
    else
    {
        bytes = (newsize + BIT_VECTOR_HIDDEN_WORDS) << FACTOR;
        newaddr = (wordptr) malloc((size_t) bytes);
        if (newaddr != NULL)
        {
            *newaddr++ = bits;
            *newaddr++ = newsize;
            *newaddr++ = newmask;
            target = newaddr;
            source = oldaddr;
            newsize -= oldsize;
            BIT_VECTOR_COPY_WORDS(target,source,oldsize)
            BIT_VECTOR_ZERO_WORDS(target,newsize)
        }
        BitVector_Destroy(oldaddr);
    }
    return(newaddr);
}

void BitVector_Destroy(wordptr addr)                        /* free          */
{
    if (addr != NULL)
    {
        addr -= BIT_VECTOR_HIDDEN_WORDS;
        free((voidptr) addr);
    }
}

void BitVector_Copy(wordptr X, wordptr Y)                           /* X = Y */
{
    N_word  sizeX = size_(X);
    N_word  sizeY = size_(Y);
    N_word  maskX = mask_(X);
    N_word  maskY = mask_(Y);
    N_word  fill  = 0;
    wordptr lastX;
    wordptr lastY;

    if ((X != Y) and (sizeX > 0))
    {
        lastX = X + sizeX - 1;
        if (sizeY > 0)
        {
            lastY = Y + sizeY - 1;
            *lastY &= maskY;
            while ((sizeX > 0) and (sizeY > 0))
            {
                *X++ = *Y++;
                sizeX--;
                sizeY--;
            }
            if ( (*lastY AND (maskY AND NOT (maskY >> 1))) != 0 )
            {
                fill = (N_word) ~0L;
                *(X-1) |= NOT maskY;
            }
        }
        while (sizeX-- > 0) *X++ = fill;
        *lastX &= maskX;
    }
}

void BitVector_Empty(wordptr addr)                        /* X = {}  clr all */
{
    N_word size = size_(addr);

    BIT_VECTOR_ZERO_WORDS(addr,size)
}

void BitVector_Fill(wordptr addr)                         /* X = ~{} set all */
{
    N_word size = size_(addr);
    N_word mask = mask_(addr);
    N_word fill = (N_word) ~0L;

    if (size > 0)
    {
        BIT_VECTOR_FILL_WORDS(addr,fill,size)
        *(--addr) &= mask;
    }
}

void BitVector_Flip(wordptr addr)                         /* X = ~X flip all */
{
    N_word size = size_(addr);
    N_word mask = mask_(addr);
    N_word flip = (N_word) ~0L;

    if (size > 0)
    {
        BIT_VECTOR_FLIP_WORDS(addr,flip,size)
        *(--addr) &= mask;
    }
}

void BitVector_Primes(wordptr addr)
{
    N_word  bits = bits_(addr);
    N_word  size = size_(addr);
    wordptr work;
    N_word  temp;
    N_word  i,j;

    if (size > 0)
    {
        temp = 0xAAAA;
        i = BITS >> 4;
        while (--i > 0)
        {
            temp <<= 16;
            temp |= 0xAAAA;
        }
        i = size;
        work = addr;
        *work++ = temp XOR 0x0006;
        while (--i > 0) *work++ = temp;
        for ( i = 3; (j = i * i) < bits; i += 2 )
        {
            for ( ; j < bits; j += i ) BIT_VECTOR_CLR_BIT(addr,j)
        }
        *(addr+size-1) &= mask_(addr);
    }
}

void BitVector_Reverse(wordptr X, wordptr Y)
{
    N_word bits = bits_(X);
    N_word mask;
    N_word bit;
    N_word value;

    if (bits > 0)
    {
        if (X == Y) BitVector_Interval_Reverse(X,0,bits-1);
        else if (bits == bits_(Y))
        {
/*          mask = mask_(Y);  */
/*          mask &= NOT (mask >> 1);  */
            mask = BITMASKTAB[(bits-1) AND MODMASK];
            Y += size_(Y) - 1;
            value = 0;
            bit = LSB;
            while (bits-- > 0)
            {
                if ((*Y AND mask) != 0)
                {
                    value |= bit;
                }
                if (not (mask >>= 1))
                {
                    Y--;
                    mask = MSB;
                }
                if (not (bit <<= 1))
                {
                    *X++ = value;
                    value = 0;
                    bit = LSB;
                }
            }
            if (bit > LSB) *X = value;
        }
    }
}

void BitVector_Interval_Empty(wordptr addr, N_int lower, N_int upper)
{                                                  /* X = X \ [lower..upper] */
    N_word  bits = bits_(addr);
    N_word  size = size_(addr);
    wordptr loaddr;
    wordptr hiaddr;
    N_word  lobase;
    N_word  hibase;
    N_word  lomask;
    N_word  himask;
    N_word  diff;

    if ((size > 0) and (lower < bits) and (upper < bits) and (lower <= upper))
    {
        lobase = lower >> LOGBITS;
        hibase = upper >> LOGBITS;
        diff = hibase - lobase;
        loaddr = addr + lobase;
        hiaddr = addr + hibase;

        lomask = (N_word)   (~0L << (lower AND MODMASK));
        himask = (N_word) ~((~0L << (upper AND MODMASK)) << 1);

        if (diff == 0)
        {
            *loaddr &= NOT (lomask AND himask);
        }
        else
        {
            *loaddr++ &= NOT lomask;
            while (--diff > 0)
            {
                *loaddr++ = 0;
            }
            *hiaddr &= NOT himask;
        }
    }
}

void BitVector_Interval_Fill(wordptr addr, N_int lower, N_int upper)
{                                                  /* X = X + [lower..upper] */
    N_word  bits = bits_(addr);
    N_word  size = size_(addr);
    N_word  fill = (N_word) ~0L;
    wordptr loaddr;
    wordptr hiaddr;
    N_word  lobase;
    N_word  hibase;
    N_word  lomask;
    N_word  himask;
    N_word  diff;

    if ((size > 0) and (lower < bits) and (upper < bits) and (lower <= upper))
    {
        lobase = lower >> LOGBITS;
        hibase = upper >> LOGBITS;
        diff = hibase - lobase;
        loaddr = addr + lobase;
        hiaddr = addr + hibase;

        lomask = (N_word)   (~0L << (lower AND MODMASK));
        himask = (N_word) ~((~0L << (upper AND MODMASK)) << 1);

        if (diff == 0)
        {
            *loaddr |= (lomask AND himask);
        }
        else
        {
            *loaddr++ |= lomask;
            while (--diff > 0)
            {
                *loaddr++ = fill;
            }
            *hiaddr |= himask;
        }
        *(addr+size-1) &= mask_(addr);
    }
}

void BitVector_Interval_Flip(wordptr addr, N_int lower, N_int upper)
{                                                  /* X = X ^ [lower..upper] */
    N_word  bits = bits_(addr);
    N_word  size = size_(addr);
    N_word  flip = (N_word) ~0L;
    wordptr loaddr;
    wordptr hiaddr;
    N_word  lobase;
    N_word  hibase;
    N_word  lomask;
    N_word  himask;
    N_word  diff;

    if ((size > 0) and (lower < bits) and (upper < bits) and (lower <= upper))
    {
        lobase = lower >> LOGBITS;
        hibase = upper >> LOGBITS;
        diff = hibase - lobase;
        loaddr = addr + lobase;
        hiaddr = addr + hibase;

        lomask = (N_word)   (~0L << (lower AND MODMASK));
        himask = (N_word) ~((~0L << (upper AND MODMASK)) << 1);

        if (diff == 0)
        {
            *loaddr ^= (lomask AND himask);
        }
        else
        {
            *loaddr++ ^= lomask;
            while (--diff > 0)
            {
                *loaddr++ ^= flip;
            }
            *hiaddr ^= himask;
        }
        *(addr+size-1) &= mask_(addr);
    }
}

void BitVector_Interval_Reverse(wordptr addr, N_int lower, N_int upper)
{
    N_word  bits = bits_(addr);
    wordptr loaddr;
    wordptr hiaddr;
    N_word  lomask;
    N_word  himask;

    if ((bits > 0) and (lower < bits) and (upper < bits) and (lower < upper))
    {
        loaddr = addr + (lower >> LOGBITS);
        hiaddr = addr + (upper >> LOGBITS);
        lomask = BITMASKTAB[lower AND MODMASK];
        himask = BITMASKTAB[upper AND MODMASK];
        for ( bits = upper - lower + 1; bits > 1; bits -= 2 )
        {
            if (((*loaddr AND lomask) != 0) XOR ((*hiaddr AND himask) != 0))
            {
                *loaddr ^= lomask;  /* swap bits only if they differ! */
                *hiaddr ^= himask;
            }
            if (not (lomask <<= 1))
            {
                lomask = LSB;
                loaddr++;
            }
            if (not (himask >>= 1))
            {
                himask = MSB;
                hiaddr--;
            }
        }
    }
}

boolean BitVector_interval_scan_inc(wordptr addr, N_int start,
                                    N_intptr min, N_intptr max)
{
    N_word  size = size_(addr);
    N_word  mask = mask_(addr);
    N_word  offset;
    N_word  bitmask;
    N_word  value;
    boolean empty;

    if ((size == 0) or (start >= bits_(addr))) return(false);

    *min = start;
    *max = start;

    offset = start >> LOGBITS;

    *(addr+size-1) &= mask;

    addr += offset;
    size -= offset;

    bitmask = BITMASKTAB[start AND MODMASK];
    mask = NOT (bitmask OR (bitmask - 1));

    value = *addr++;
    if ((value AND bitmask) == 0)
    {
        value &= mask;
        if (value == 0)
        {
            offset++;
            empty = true;
            while (empty and (--size > 0))
            {
                if (value = *addr++) empty = false; else offset++;
            }
            if (empty) return(false);
        }
        start = offset << LOGBITS;
        bitmask = LSB;
        mask = value;
        while (not (mask AND LSB))
        {
            bitmask <<= 1;
            mask >>= 1;
            start++;
        }
        mask = NOT (bitmask OR (bitmask - 1));
        *min = start;
        *max = start;
    }
    value = NOT value;
    value &= mask;
    if (value == 0)
    {
        offset++;
        empty = true;
        while (empty and (--size > 0))
        {
            if (value = NOT *addr++) empty = false; else offset++;
        }
        if (empty) value = LSB;
    }
    start = offset << LOGBITS;
    while (not (value AND LSB))
    {
        value >>= 1;
        start++;
    }
    *max = --start;
    return(true);
}

boolean BitVector_interval_scan_dec(wordptr addr, N_int start,
                                    N_intptr min, N_intptr max)
{
    N_word  size = size_(addr);
    N_word  mask = mask_(addr);
    N_word offset;
    N_word bitmask;
    N_word value;
    boolean empty;

    if ((size == 0) or (start >= bits_(addr))) return(false);

    *min = start;
    *max = start;

    offset = start >> LOGBITS;

    if (offset >= size) return(false);

    *(addr+size-1) &= mask;

    addr += offset;
    size = ++offset;

    bitmask = BITMASKTAB[start AND MODMASK];
    mask = (bitmask - 1);

    value = *addr--;
    if ((value AND bitmask) == 0)
    {
        value &= mask;
        if (value == 0)
        {
            offset--;
            empty = true;
            while (empty and (--size > 0))
            {
                if (value = *addr--) empty = false; else offset--;
            }
            if (empty) return(false);
        }
        start = offset << LOGBITS;
        bitmask = MSB;
        mask = value;
        while (not (mask AND MSB))
        {
            bitmask >>= 1;
            mask <<= 1;
            start--;
        }
        mask = (bitmask - 1);
        *max = --start;
        *min = start;
    }
    value = NOT value;
    value &= mask;
    if (value == 0)
    {
        offset--;
        empty = true;
        while (empty and (--size > 0))
        {
            if (value = NOT *addr--) empty = false; else offset--;
        }
        if (empty) value = MSB;
    }
    start = offset << LOGBITS;
    while (not (value AND MSB))
    {
        value <<= 1;
        start--;
    }
    *min = start;
    return(true);
}

void BitVector_Interval_Copy(wordptr X, wordptr Y, N_int Xoffset,
                             N_int Yoffset, N_int length)
{
    N_word  bitsX = bits_(X);
    N_word  bitsY = bits_(Y);
    N_word  source;
    N_word  target;
    N_word  s_lo_base;
    N_word  s_hi_base;
    N_word  s_lo_bit;
    N_word  s_hi_bit;
    N_word  s_base;
    N_word  s_lower;
    N_word  s_upper;
    N_word  s_bits;
    N_word  s_min;
    N_word  s_max;
    N_word  t_lo_base;
    N_word  t_hi_base;
    N_word  t_lo_bit;
    N_word  t_hi_bit;
    N_word  t_base;
    N_word  t_lower;
    N_word  t_upper;
    N_word  t_bits;
    N_word  t_min;
    N_word  mask;
    N_word  bits;
    N_word  select;
    boolean ascending;
    boolean notfirst;
    wordptr Z = X;

    if ((length > 0) and (Xoffset < bitsX) and (Yoffset < bitsY))
    {
        if ((Xoffset + length) > bitsX) length = bitsX - Xoffset;
        if ((Yoffset + length) > bitsY) length = bitsY - Yoffset;

        ascending = (Xoffset <= Yoffset);

        s_lo_base = Yoffset >> LOGBITS;
        s_lo_bit = Yoffset AND MODMASK;
        Yoffset += --length;
        s_hi_base = Yoffset >> LOGBITS;
        s_hi_bit = Yoffset AND MODMASK;

        t_lo_base = Xoffset >> LOGBITS;
        t_lo_bit = Xoffset AND MODMASK;
        Xoffset += length;
        t_hi_base = Xoffset >> LOGBITS;
        t_hi_bit = Xoffset AND MODMASK;

        if (ascending)
        {
            s_base = s_lo_base;
            t_base = t_lo_base;
        }
        else
        {
            s_base = s_hi_base;
            t_base = t_hi_base;
        }
        s_bits = 0;
        t_bits = 0;
        Y += s_base;
        X += t_base;
        notfirst = false;
        while (true)
        {
            if (t_bits == 0)
            {
                if (notfirst)
                {
                    *X = target;
                    if (ascending)
                    {
                        if (t_base == t_hi_base) break;
                        t_base++;
                        X++;
                    }
                    else
                    {
                        if (t_base == t_lo_base) break;
                        t_base--;
                        X--;
                    }
                }
                select = ((t_base == t_hi_base) << 1) OR (t_base == t_lo_base);
                switch (select)
                {
                    case 0:
                        t_lower = 0;
                        t_upper = BITS - 1;
                        t_bits = BITS;
                        target = 0;
                        break;
                    case 1:
                        t_lower = t_lo_bit;
                        t_upper = BITS - 1;
                        t_bits = BITS - t_lo_bit;
                        mask = (N_word) (~0L << t_lower);
                        target = *X AND NOT mask;
                        break;
                    case 2:
                        t_lower = 0;
                        t_upper = t_hi_bit;
                        t_bits = t_hi_bit + 1;
                        mask = (N_word) ((~0L << t_upper) << 1);
                        target = *X AND mask;
                        break;
                    case 3:
                        t_lower = t_lo_bit;
                        t_upper = t_hi_bit;
                        t_bits = t_hi_bit - t_lo_bit + 1;
                        mask = (N_word) (~0L << t_lower);
                        mask &= (N_word) ~((~0L << t_upper) << 1);
                        target = *X AND NOT mask;
                        break;
                }
            }
            if (s_bits == 0)
            {
                if (notfirst)
                {
                    if (ascending)
                    {
                        if (s_base == s_hi_base) break;
                        s_base++;
                        Y++;
                    }
                    else
                    {
                        if (s_base == s_lo_base) break;
                        s_base--;
                        Y--;
                    }
                }
                source = *Y;
                select = ((s_base == s_hi_base) << 1) OR (s_base == s_lo_base);
                switch (select)
                {
                    case 0:
                        s_lower = 0;
                        s_upper = BITS - 1;
                        s_bits = BITS;
                        break;
                    case 1:
                        s_lower = s_lo_bit;
                        s_upper = BITS - 1;
                        s_bits = BITS - s_lo_bit;
                        break;
                    case 2:
                        s_lower = 0;
                        s_upper = s_hi_bit;
                        s_bits = s_hi_bit + 1;
                        break;
                    case 3:
                        s_lower = s_lo_bit;
                        s_upper = s_hi_bit;
                        s_bits = s_hi_bit - s_lo_bit + 1;
                        break;
                }
            }
            notfirst = true;
            if (s_bits > t_bits)
            {
                bits = t_bits - 1;
                if (ascending)
                {
                    s_min = s_lower;
                    s_max = s_lower + bits;
                }
                else
                {
                    s_max = s_upper;
                    s_min = s_upper - bits;
                }
                t_min = t_lower;
            }
            else
            {
                bits = s_bits - 1;
                if (ascending) t_min = t_lower;
                else           t_min = t_upper - bits;
                s_min = s_lower;
                s_max = s_upper;
            }
            bits++;
            mask = (N_word) (~0L << s_min);
            mask &= (N_word) ~((~0L << s_max) << 1);
            if (s_min == t_min) target |= (source AND mask);
            else
            {
                if (s_min < t_min) target |= (source AND mask) << (t_min-s_min);
                else               target |= (source AND mask) >> (s_min-t_min);
            }
            if (ascending)
            {
                s_lower += bits;
                t_lower += bits;
            }
            else
            {
                s_upper -= bits;
                t_upper -= bits;
            }
            s_bits -= bits;
            t_bits -= bits;
        }
        *(Z+size_(Z)-1) &= mask_(Z);
    }
}


wordptr BitVector_Interval_Substitute(wordptr X, wordptr Y,
                                      N_int Xoffset, N_int Xlength,
                                      N_int Yoffset, N_int Ylength)
{
    N_word Xbits = bits_(X);
    N_word Ybits = bits_(Y);
    N_word limit;
    N_word diff;

    if ((Xoffset <= Xbits) and (Yoffset <= Ybits))
    {
        limit = Xoffset + Xlength;
        if (limit > Xbits)
        {
            limit = Xbits;
            Xlength = Xbits - Xoffset;
        }
        if ((Yoffset + Ylength) > Ybits)
        {
            Ylength = Ybits - Yoffset;
        }
        if (Xlength == Ylength)
        {
            if ((Ylength > 0) and ((X != Y) or (Xoffset != Yoffset)))
            {
                BitVector_Interval_Copy(X,Y,Xoffset,Yoffset,Ylength);
            }
        }
        else /* Xlength != Ylength */
        {
            if (Xlength > Ylength)
            {
                diff = Xlength - Ylength;
                if (Ylength > 0) BitVector_Interval_Copy(X,Y,Xoffset,Yoffset,Ylength);
                if (limit < Xbits) BitVector_Delete(X,Xoffset+Ylength,diff,false);
                if ((X = BitVector_Resize(X,Xbits-diff)) == NULL) return(NULL);
            }
            else /* Ylength > Xlength  ==>  Ylength > 0 */
            {
                diff = Ylength - Xlength;
                if (X != Y)
                {
                    if ((X = BitVector_Resize(X,Xbits+diff)) == NULL) return(NULL);
                    if (limit < Xbits) BitVector_Insert(X,limit,diff,false);
                    BitVector_Interval_Copy(X,Y,Xoffset,Yoffset,Ylength);
                }
                else /* in-place */
                {
                    if ((Y = X = BitVector_Resize(X,Xbits+diff)) == NULL) return(NULL);
                    if (limit >= Xbits)
                    {
                        BitVector_Interval_Copy(X,Y,Xoffset,Yoffset,Ylength);
                    }
                    else /* limit < Xbits */
                    {
                        BitVector_Insert(X,limit,diff,false);
                        if ((Yoffset+Ylength) <= limit)
                        {
                            BitVector_Interval_Copy(X,Y,Xoffset,Yoffset,Ylength);
                        }
                        else /* overlaps or lies above critical area */
                        {
                            if (limit <= Yoffset)
                            {
                                Yoffset += diff;
                                BitVector_Interval_Copy(X,Y,Xoffset,Yoffset,Ylength);
                            }
                            else /* Yoffset < limit */
                            {
                                Xlength = limit - Yoffset;
                                BitVector_Interval_Copy(X,Y,Xoffset,Yoffset,Xlength);
                                Yoffset = Xoffset + Ylength; /* = limit + diff */
                                Xoffset += Xlength;
                                Ylength -= Xlength;
                                BitVector_Interval_Copy(X,Y,Xoffset,Yoffset,Ylength);
                            }
                        }
                    }
                }
            }
        }
    }
    return(X);
}

boolean BitVector_is_empty(wordptr addr)                    /* X == {} ?     */
{
    N_word  size = size_(addr);
    boolean r = true;

    if (size > 0)
    {
        *(addr+size-1) &= mask_(addr);
        while (r and (size-- > 0)) r = ( *addr++ == 0 );
    }
    return(r);
}

boolean BitVector_is_full(wordptr addr)                     /* X == ~{} ?    */
{
    N_word  size = size_(addr);
    N_word  mask = mask_(addr);
    boolean r = false;
    wordptr last;

    if (size > 0)
    {
        r = true;
        last = addr + size - 1;
        *last |= NOT mask;
        while (r and (size-- > 0)) r = ( NOT *addr++ == 0 );
        *last &= mask;
    }
    return(r);
}

boolean BitVector_equal(wordptr X, wordptr Y)               /* X == Y ?      */
{
    N_word  size = size_(X);
    N_word  mask = mask_(X);
    boolean r = false;

    if (bits_(X) == bits_(Y))
    {
        r = true;
        if (size > 0)
        {
            *(X+size-1) &= mask;
            *(Y+size-1) &= mask;
            while (r and (size-- > 0)) r = (*X++ == *Y++);
        }
    }
    return(r);
}

Z_int BitVector_Lexicompare(wordptr X, wordptr Y)           /* X <,=,> Y ?   */
{                                                           /*  unsigned     */
    N_word  bitsX = bits_(X);
    N_word  bitsY = bits_(Y);
    N_word  size  = size_(X);
    boolean r = true;

    if (bitsX == bitsY)
    {
        if (size > 0)
        {
            X += size;
            Y += size;
            while (r and (size-- > 0)) r = (*(--X) == *(--Y));
        }
        if (r) return((Z_int) 0);
        else
        {
            if (*X < *Y) return((Z_int) -1); else return((Z_int) 1);
        }
    }
    else
    {
        if (bitsX < bitsY) return((Z_int) -1); else return((Z_int) 1);
    }
}

Z_int BitVector_Compare(wordptr X, wordptr Y)               /* X <,=,> Y ?   */
{                                                           /*   signed      */
    N_word  bitsX = bits_(X);
    N_word  bitsY = bits_(Y);
    N_word  size  = size_(X);
    N_word  mask  = mask_(X);
    N_word  sign;
    boolean r = true;

    if (bitsX == bitsY)
    {
        if (size > 0)
        {
            X += size;
            Y += size;
            mask &= NOT (mask >> 1);
            if ((sign = (*(X-1) AND mask)) != (*(Y-1) AND mask))
            {
                if (sign) return((Z_int) -1); else return((Z_int) 1);
            }
            while (r and (size-- > 0)) r = (*(--X) == *(--Y));
        }
        if (r) return((Z_int) 0);
        else
        {
            if (*X < *Y) return((Z_int) -1); else return((Z_int) 1);
        }
    }
    else
    {
        if (bitsX < bitsY) return((Z_int) -1); else return((Z_int) 1);
    }
}

charptr BitVector_to_Hex(wordptr addr)
{
    N_word  bits = bits_(addr);
    N_word  size = size_(addr);
    N_word  value;
    N_word  count;
    N_word  digit;
    N_word  length;
    charptr string;

    length = bits >> 2;
    if (bits AND 0x0003) length++;
    string = (charptr) malloc((size_t) (length+1));
    if (string == NULL) return(NULL);
    string += length;
    *string = (N_char) '\0';
    if (size > 0)
    {
        *(addr+size-1) &= mask_(addr);
        while ((size-- > 0) and (length > 0))
        {
            value = *addr++;
            count = BITS >> 2;
            while ((count-- > 0) and (length > 0))
            {
                digit = value AND 0x000F;
                if (digit > 9) digit += (N_word) 'A' - 10;
                else           digit += (N_word) '0';
                *(--string) = (N_char) digit; length--;
                if ((count > 0) and (length > 0)) value >>= 4;
            }
        }
    }
    return(string);
}

ErrCode BitVector_from_Hex(wordptr addr, charptr string)
{
    N_word  size = size_(addr);
    N_word  mask = mask_(addr);
    boolean ok = true;
    N_word  length;
    N_word  value;
    N_word  count;
    int     digit;

    if (size > 0)
    {
        length = strlen((char *) string);
        string += length;
        while (size-- > 0)
        {
            value = 0;
            for ( count = 0; (ok and (length > 0) and (count < BITS)); count += 4 )
            {
                digit = (int) *(--string); length--;
                /* separate because toupper() is likely a macro! */
                digit = toupper(digit);
                if (ok = (isxdigit(digit) != 0))
                {
                    if (digit >= (int) 'A') digit -= (int) 'A' - 10;
                    else                    digit -= (int) '0';
                    value |= (((N_word) digit) << count);
                }
            }
            *addr++ = value;
        }
        *(--addr) &= mask;
    }
    if (ok) return(ErrCode_Ok);
    else    return(ErrCode_Pars);
}

charptr BitVector_to_Bin(wordptr addr)
{
    N_word  size = size_(addr);
    N_word  value;
    N_word  count;
    N_word  digit;
    N_word  length;
    charptr string;

    length = bits_(addr);
    string = (charptr) malloc((size_t) (length+1));
    if (string == NULL) return(NULL);
    string += length;
    *string = (N_char) '\0';
    if (size > 0)
    {
        *(addr+size-1) &= mask_(addr);
        while (size-- > 0)
        {
            value = *addr++;
            count = BITS;
            if (count > length) count = length;
            while (count-- > 0)
            {
                digit = value AND 0x0001;
                digit += (N_word) '0';
                *(--string) = (N_char) digit; length--;
                if (count > 0) value >>= 1;
            }
        }
    }
    return(string);
}

ErrCode BitVector_from_Bin(wordptr addr, charptr string)
{
    N_word  size = size_(addr);
    N_word  mask = mask_(addr);
    boolean ok = true;
    N_word  length;
    N_word  value;
    N_word  count;
    int     digit;

    if (size > 0)
    {
        length = strlen((char *) string);
        string += length;
        while (size-- > 0)
        {
            value = 0;
            for ( count = 0; (ok and (length > 0) and (count < BITS)); count++ )
            {
                digit = (int) *(--string); length--;
                switch (digit)
                {
                    case (int) '0':
                        break;
                    case (int) '1':
                        value |= BITMASKTAB[count];
                        break;
                    default:
                        ok = false;
                        break;
                }
            }
            *addr++ = value;
        }
        *(--addr) &= mask;
    }
    if (ok) return(ErrCode_Ok);
    else    return(ErrCode_Pars);
}

charptr BitVector_to_Dec(wordptr addr)
{
    N_word  bits = bits_(addr);
    N_word  length;
    N_word  digits;
    N_word  count;
    N_word  q;
    N_word  r;
    boolean loop;
    charptr result;
    charptr string;
    wordptr quot;
    wordptr rest;
    wordptr temp;
    wordptr base;
    Z_int   sign;

    length = (N_word) (bits / 3.3);        /* digits = bits * ln(2) / ln(10) */
    length += 2; /* compensate for truncating & provide space for minus sign */
    result = (charptr) malloc((size_t) (length+1));    /* remember the '\0'! */
    if (result == NULL) return(NULL);
    string = result;
    sign = BitVector_Sign(addr);
    if ((bits < 4) or (sign == 0))
    {
        if (bits > 0) digits = *addr; else digits = (N_word) 0;
        if (sign < 0) digits = -digits AND mask_(addr);
        *string++ = (N_char) digits + (N_char) '0';
        digits = 1;
    }
    else
    {
        quot = BitVector_Create(bits,false);
        if (quot == NULL)
        {
            BitVector_Dispose(result);
            return(NULL);
        }
        rest = BitVector_Create(bits,false);
        if (rest == NULL)
        {
            BitVector_Dispose(result);
            BitVector_Destroy(quot);
            return(NULL);
        }
        temp = BitVector_Create(bits,false);
        if (temp == NULL)
        {
            BitVector_Dispose(result);
            BitVector_Destroy(quot);
            BitVector_Destroy(rest);
            return(NULL);
        }
        base = BitVector_Create(bits,true);
        if (base == NULL)
        {
            BitVector_Dispose(result);
            BitVector_Destroy(quot);
            BitVector_Destroy(rest);
            BitVector_Destroy(temp);
            return(NULL);
        }
        if (sign < 0) BitVector_Negate(quot,addr);
        else           BitVector_Copy(quot,addr);
        digits = 0;
        *base = EXP10;
        loop = (bits >= BITS);
        do
        {
            if (loop)
            {
                BitVector_Copy(temp,quot);
                if (BitVector_Div_Pos(quot,temp,base,rest))
                {
                    BitVector_Dispose(result); /* emergency exit */
                    BitVector_Destroy(quot);
                    BitVector_Destroy(rest);   /* should never occur */
                    BitVector_Destroy(temp);   /* under normal operation */
                    BitVector_Destroy(base);
                    return(NULL);
                }
                loop = not BitVector_is_empty(quot);
                q = *rest;
            }
            else q = *quot;
            count = LOG10;
            while (((loop and (count-- > 0)) or ((not loop) and (q != 0))) and
                (digits < length))
            {
                if (q != 0)
                {
                    BIT_VECTOR_DIGITIZE(N_word,q,r)
                }
                else r = (N_word) '0';
                *string++ = (N_char) r;
                digits++;
            }
        }
        while (loop and (digits < length));
        BitVector_Destroy(quot);
        BitVector_Destroy(rest);
        BitVector_Destroy(temp);
        BitVector_Destroy(base);
    }
    if ((sign < 0) and (digits < length))
    {
        *string++ = (N_char) '-';
        digits++;
    }
    *string = (N_char) '\0';
    BIT_VECTOR_reverse(result,digits);
    return(result);
}

ErrCode BitVector_from_Dec(wordptr addr, charptr string)
{
    ErrCode error = ErrCode_Ok;
    N_word  bits = bits_(addr);
    N_word  mask = mask_(addr);
    boolean init = (bits > BITS);
    boolean minus;
    boolean shift;
    wordptr term;
    wordptr base;
    wordptr prod;
    wordptr rank;
    wordptr temp;
    wordptr last;
    N_word  msb;
    N_word  prev;
    N_word  accu;
    N_word  powr;
    N_word  count;
    N_word  length;
    int     digit;

    if (bits > 0)
    {
        length = strlen((char *) string);
        if (length == 0) return(ErrCode_Pars);
        digit = (int) *string;
        if ((minus = (digit == (int) '-')) or
                     (digit == (int) '+'))
        {
            string++;
            if (--length == 0) return(ErrCode_Pars);
        }
        string += length;
        term = BitVector_Create(BITS,false);
        if (term == NULL)
        {
            return(ErrCode_Null);
        }
        base = BitVector_Create(BITS,false);
        if (base == NULL)
        {
            BitVector_Destroy(term);
            return(ErrCode_Null);
        }
        prod = BitVector_Create(bits,init);
        if (prod == NULL)
        {
            BitVector_Destroy(term);
            BitVector_Destroy(base);
            return(ErrCode_Null);
        }
        rank = BitVector_Create(bits,init);
        if (rank == NULL)
        {
            BitVector_Destroy(term);
            BitVector_Destroy(base);
            BitVector_Destroy(prod);
            return(ErrCode_Null);
        }
        temp = BitVector_Create(bits,false);
        if (temp == NULL)
        {
            BitVector_Destroy(term);
            BitVector_Destroy(base);
            BitVector_Destroy(prod);
            BitVector_Destroy(rank);
            return(ErrCode_Null);
        }
        last = addr + size_(addr) - 1;
        msb = mask AND NOT (mask >> 1);
        BitVector_Empty(addr);
        *base = EXP10;
        shift = false;
        while ((not error) and (length > 0))
        {
            accu = 0;
            powr = 1;
            count = LOG10;
            while ((not error) and (length > 0) and (count-- > 0))
            {
                digit = (int) *(--string); length--;
                /* separate because isdigit() is likely a macro! */
                if (isdigit(digit) != 0)
                {
                    accu += ((N_word) digit - (N_word) '0') * powr;
                    powr *= 10;
                }
                else error = ErrCode_Pars;
            }
            if (not error)
            {
                if (shift)
                {
                    *term = accu;
                    BitVector_Copy(temp,rank);
                    error = BitVector_Mul_Pos(prod,temp,term);
                }
                else
                {
                    *prod = accu;
                    if ((not init) and ((accu AND NOT mask) != 0)) error = ErrCode_Ovfl;
                }
                if (not error)
                {
                    prev = *last AND msb;
                    if (BitVector_add(addr,addr,prod,0) or ((*last AND msb) != prev))
                    {
                        *last ^= msb;
                        if (BitVector_is_empty(addr)) *last ^= msb;
                        else error = ErrCode_Ovfl;
                    }
                    else if (length > 0)
                    {
                        if (shift)
                        {
                            BitVector_Copy(temp,rank);
                            error = BitVector_Mul_Pos(rank,temp,base);
                        }
                        else
                        {
                            *rank = *base;
                            shift = true;
                        }
                    }
                }
            }
        }
        BitVector_Destroy(term);
        BitVector_Destroy(base);
        BitVector_Destroy(prod);
        BitVector_Destroy(rank);
        BitVector_Destroy(temp);
        if (not error)
        {
            if (minus) BitVector_Negate(addr,addr);
            if (minus XOR ((*last AND msb) != 0)) error = ErrCode_Ovfl;
        }
    }
    return(error);
}

charptr BitVector_to_Enum(wordptr addr)
{
    N_word  bits = bits_(addr);
    N_word  sample;
    N_word  length;
    N_word  digits;
    N_word  factor;
    N_word  power;
    N_word  start;
    N_word  min;
    N_word  max;
    charptr string;
    charptr target;
    boolean comma;

    if (bits > 0)
    {
        sample = bits - 1;  /* greatest possible index */
        length = 2;         /* account for index 0 and terminating '\0' */
        digits = 1;         /* account for intervening dashes and commas */
        factor = 1;
        power = 10;
        while (sample >= (power-1))
        {
            length += ++digits * factor * 6;  /* 9,90,900,9000,... (9*2/3 = 6) */
            factor = power;
            power *= 10;
        }
        if (sample > --factor)
        {
            sample -= factor;
            factor = (N_word) ( sample / 3 );
            factor = (factor << 1) + (sample - (factor * 3));
            length += ++digits * factor;
        }
    }
    else length = 1;
    string = (charptr) malloc((size_t) length);
    if (string == NULL) return(NULL);
    start = 0;
    comma = false;
    target = string;
    while ((start < bits) and BitVector_interval_scan_inc(addr,start,&min,&max))
    {
        start = max + 2;
        if (comma) *target++ = (N_char) ',';
        if (min == max)
        {
            target += BIT_VECTOR_int2str(target,min);
        }
        else
        {
            if (min+1 == max)
            {
                target += BIT_VECTOR_int2str(target,min);
                *target++ = (N_char) ',';
                target += BIT_VECTOR_int2str(target,max);
            }
            else
            {
                target += BIT_VECTOR_int2str(target,min);
                *target++ = (N_char) '-';
                target += BIT_VECTOR_int2str(target,max);
            }
        }
        comma = true;
    }
    *target = (N_char) '\0';
    return(string);
}

ErrCode BitVector_from_Enum(wordptr addr, charptr string)
{
    ErrCode error = ErrCode_Ok;
    N_word  bits = bits_(addr);
    N_word  state = 1;
    N_word  token;
    N_word  index;
    N_word  start;

    if (bits > 0)
    {
        BitVector_Empty(addr);
        while ((not error) and (state != 0))
        {
            token = (N_word) *string;
            /* separate because isdigit() is likely a macro! */
            if (isdigit(token) != 0)
            {
                string += BIT_VECTOR_str2int(string,&index);
                if (index < bits) token = (N_word) '0';
                else error = ErrCode_Indx;
            }
            else string++;
            if (not error)
            switch (state)
            {
                case 1:
                    switch (token)
                    {
                        case (N_word) '0':
                            state = 2;
                            break;
                        case (N_word) '\0':
                            state = 0;
                            break;
                        default:
                            error = ErrCode_Pars;
                            break;
                    }
                    break;
                case 2:
                    switch (token)
                    {
                        case (N_word) '-':
                            start = index;
                            state = 3;
                            break;
                        case (N_word) ',':
                            BIT_VECTOR_SET_BIT(addr,index)
                            state = 5;
                            break;
                        case (N_word) '\0':
                            BIT_VECTOR_SET_BIT(addr,index)
                            state = 0;
                            break;
                        default:
                            error = ErrCode_Pars;
                            break;
                    }
                    break;
                case 3:
                    switch (token)
                    {
                        case (N_word) '0':
                            if (start < index)
                                BitVector_Interval_Fill(addr,start,index);
                            else if (start == index)
                                BIT_VECTOR_SET_BIT(addr,index)
                            else error = ErrCode_Ordr;
                            state = 4;
                            break;
                        default:
                            error = ErrCode_Pars;
                            break;
                    }
                    break;
                case 4:
                    switch (token)
                    {
                        case (N_word) ',':
                            state = 5;
                            break;
                        case (N_word) '\0':
                            state = 0;
                            break;
                        default:
                            error = ErrCode_Pars;
                            break;
                    }
                    break;
                case 5:
                    switch (token)
                    {
                        case (N_word) '0':
                            state = 2;
                            break;
                        default:
                            error = ErrCode_Pars;
                            break;
                    }
                    break;
            }
        }
    }
    return(error);
}

void BitVector_Dispose(charptr string)
{
    if (string != NULL) free((voidptr) string);
}

void BitVector_Bit_Off(wordptr addr, N_int index)           /* X = X \ {x}   */
{
    if (index < bits_(addr)) BIT_VECTOR_CLR_BIT(addr,index)
}

void BitVector_Bit_On(wordptr addr, N_int index)            /* X = X + {x}   */
{
    if (index < bits_(addr)) BIT_VECTOR_SET_BIT(addr,index)
}

boolean BitVector_bit_flip(wordptr addr, N_int index)   /* X=(X+{x})\(X*{x}) */
{
    N_word mask;

    if (index < bits_(addr)) return( BIT_VECTOR_FLP_BIT(addr,index,mask) );
    else                     return( false );
}

boolean BitVector_bit_test(wordptr addr, N_int index)       /* {x} in X ?    */
{
    if (index < bits_(addr)) return( BIT_VECTOR_TST_BIT(addr,index) );
    else                     return( false );
}

void BitVector_Bit_Copy(wordptr addr, N_int index, boolean bit)
{
    if (index < bits_(addr))
    {
        if (bit) BIT_VECTOR_SET_BIT(addr,index)
        else     BIT_VECTOR_CLR_BIT(addr,index)
    }
}

void BitVector_LSB(wordptr addr, boolean bit)
{
    if (bits_(addr) > 0)
    {
        if (bit) *addr |= LSB;
        else     *addr &= NOT LSB;
    }
}

void BitVector_MSB(wordptr addr, boolean bit)
{
    N_word size = size_(addr);
    N_word mask = mask_(addr);

    if (size-- > 0)
    {
        if (bit) *(addr+size) |= mask AND NOT (mask >> 1);
        else     *(addr+size) &= NOT mask OR (mask >> 1);
    }
}

boolean BitVector_lsb(wordptr addr)
{
    if (size_(addr) > 0) return( (*addr AND LSB) != 0 );
    else                 return( false );
}

boolean BitVector_msb(wordptr addr)
{
    N_word size = size_(addr);
    N_word mask = mask_(addr);

    if (size-- > 0)
        return( (*(addr+size) AND (mask AND NOT (mask >> 1))) != 0 );
    else
        return( false );
}

boolean BitVector_rotate_left(wordptr addr)
{
    N_word  size = size_(addr);
    N_word  mask = mask_(addr);
    N_word  msb;
    boolean carry_in;
    boolean carry_out = false;

    if (size > 0)
    {
        msb = mask AND NOT (mask >> 1);
        carry_in = ((*(addr+size-1) AND msb) != 0);
        while (size-- > 1)
        {
            carry_out = ((*addr AND MSB) != 0);
            *addr <<= 1;
            if (carry_in) *addr |= LSB;
            carry_in = carry_out;
            addr++;
        }
        carry_out = ((*addr AND msb) != 0);
        *addr <<= 1;
        if (carry_in) *addr |= LSB;
        *addr &= mask;
    }
    return(carry_out);
}

boolean BitVector_rotate_right(wordptr addr)
{
    N_word  size = size_(addr);
    N_word  mask = mask_(addr);
    N_word  msb;
    boolean carry_in;
    boolean carry_out = false;

    if (size > 0)
    {
        msb = mask AND NOT (mask >> 1);
        carry_in = ((*addr AND LSB) != 0);
        addr += size-1;
        *addr &= mask;
        carry_out = ((*addr AND LSB) != 0);
        *addr >>= 1;
        if (carry_in) *addr |= msb;
        carry_in = carry_out;
        addr--;
        size--;
        while (size-- > 0)
        {
            carry_out = ((*addr AND LSB) != 0);
            *addr >>= 1;
            if (carry_in) *addr |= MSB;
            carry_in = carry_out;
            addr--;
        }
    }
    return(carry_out);
}

boolean BitVector_shift_left(wordptr addr, boolean carry_in)
{
    N_word  size = size_(addr);
    N_word  mask = mask_(addr);
    N_word  msb;
    boolean carry_out = carry_in;

    if (size > 0)
    {
        msb = mask AND NOT (mask >> 1);
        while (size-- > 1)
        {
            carry_out = ((*addr AND MSB) != 0);
            *addr <<= 1;
            if (carry_in) *addr |= LSB;
            carry_in = carry_out;
            addr++;
        }
        carry_out = ((*addr AND msb) != 0);
        *addr <<= 1;
        if (carry_in) *addr |= LSB;
        *addr &= mask;
    }
    return(carry_out);
}

boolean BitVector_shift_right(wordptr addr, boolean carry_in)
{
    N_word  size = size_(addr);
    N_word  mask = mask_(addr);
    N_word  msb;
    boolean carry_out = carry_in;

    if (size > 0)
    {
        msb = mask AND NOT (mask >> 1);
        addr += size-1;
        *addr &= mask;
        carry_out = ((*addr AND LSB) != 0);
        *addr >>= 1;
        if (carry_in) *addr |= msb;
        carry_in = carry_out;
        addr--;
        size--;
        while (size-- > 0)
        {
            carry_out = ((*addr AND LSB) != 0);
            *addr >>= 1;
            if (carry_in) *addr |= MSB;
            carry_in = carry_out;
            addr--;
        }
    }
    return(carry_out);
}

void BitVector_Move_Left(wordptr addr, N_int bits)
{
    N_word count;
    N_word words;

    if (bits > 0)
    {
        count = bits AND MODMASK;
        words = bits >> LOGBITS;
        if (bits >= bits_(addr)) BitVector_Empty(addr);
        else
        {
            while (count-- > 0) BitVector_shift_left(addr,0);
            BitVector_Word_Insert(addr,0,words,true);
        }
    }
}

void BitVector_Move_Right(wordptr addr, N_int bits)
{
    N_word count;
    N_word words;

    if (bits > 0)
    {
        count = bits AND MODMASK;
        words = bits >> LOGBITS;
        if (bits >= bits_(addr)) BitVector_Empty(addr);
        else
        {
            while (count-- > 0) BitVector_shift_right(addr,0);
            BitVector_Word_Delete(addr,0,words,true);
        }
    }
}

void BitVector_Insert(wordptr addr, N_int offset, N_int count, boolean clear)
{
    N_word bits = bits_(addr);
    N_word last;

    if ((count > 0) and (offset < bits))
    {
        last = offset + count;
        if (last < bits)
        {
            BitVector_Interval_Copy(addr,addr,last,offset,(bits-last));
        }
        else last = bits;
        if (clear) BitVector_Interval_Empty(addr,offset,(last-1));
    }
}

void BitVector_Delete(wordptr addr, N_int offset, N_int count, boolean clear)
{
    N_word bits = bits_(addr);
    N_word last;

    if ((count > 0) and (offset < bits))
    {
        last = offset + count;
        if (last < bits)
        {
            BitVector_Interval_Copy(addr,addr,offset,last,(bits-last));
        }
        else count = bits - offset;
        if (clear) BitVector_Interval_Empty(addr,(bits-count),(bits-1));
    }
}

boolean BitVector_increment(wordptr addr)                   /* X++           */
{
    N_word  size  = size_(addr);
    N_word  mask  = mask_(addr);
    wordptr last  = addr + size - 1;
    boolean carry = true;

    if (size > 0)
    {
        *last |= NOT mask;
        while (carry and (size-- > 0))
        {
            carry = (++(*addr++) == 0);
        }
        *last &= mask;
    }
    return(carry);
}

boolean BitVector_decrement(wordptr addr)                   /* X--           */
{
    N_word  size  = size_(addr);
    N_word  mask  = mask_(addr);
    wordptr last  = addr + size - 1;
    boolean carry = true;

    if (size > 0)
    {
        *last &= mask;
        while (carry and (size-- > 0))
        {
            carry = (*addr == 0);
            --(*addr++);
        }
        *last &= mask;
    }
    return(carry);
}

boolean BitVector_add(wordptr X, wordptr Y, wordptr Z, boolean carry)
{
    N_word size = size_(X);
    N_word mask = mask_(X);
    N_word yy;
    N_word zz;
    N_word lo;
    N_word hi;

    if (size > 0)
    {
        while (size-- > 0)
        {
            yy = *Y++;
            zz = *Z++;
            if (size == 0) { yy &= mask; zz &= mask; }
            lo = (yy AND LSB) + (zz AND LSB) + (carry AND LSB);
            hi = (yy >> 1) + (zz >> 1) + (lo >> 1);
            carry = ((hi AND MSB) != 0);
            *X++ = (hi << 1) OR (lo AND LSB);
        }
        X--;
        if (NOT mask) carry = ((*X AND (mask+1)) != 0);
        *X &= mask;
    }
    return(carry);
}

boolean BitVector_subtract(wordptr X, wordptr Y, wordptr Z, boolean carry)
{
    N_word size = size_(X);
    N_word mask = mask_(X);
    N_word  yy;
    N_word  zz;
    N_word  lo;
    N_word  hi;

    if (size > 0)
    {
        carry = not carry;
        while (size-- > 0)
        {
            yy = *Y++;
            zz = NOT *Z++;
            if (size == 0) { yy &= mask; zz &= mask; }
            lo = (yy AND LSB) + (zz AND LSB) + (carry AND LSB);
            hi = (yy >> 1) + (zz >> 1) + (lo >> 1);
            carry = ((hi AND MSB) != 0);
            *X++ = (hi << 1) OR (lo AND LSB);
        }
        X--;
        if (NOT mask) carry = ((*X AND (mask+1)) != 0);
        *X &= mask;
        carry = not carry;
    }
    return(carry);
}

void BitVector_Negate(wordptr X, wordptr Y)
{
    N_word  size  = size_(X);
    N_word  mask  = mask_(X);
    boolean carry = true;

    if (size > 0)
    {
        while (size-- > 0)
        {
            *X = NOT *Y++;
            if (carry)
            {
                carry = (++(*X) == 0);
            }
            X++;
        }
        *(--X) &= mask;
    }
}

void BitVector_Absolute(wordptr X, wordptr Y)
{
    N_word size = size_(Y);
    N_word mask = mask_(Y);

    if (size > 0)
    {
        if (*(Y+size-1) AND (mask AND NOT (mask >> 1))) BitVector_Negate(X,Y);
        else                                             BitVector_Copy(X,Y);
    }
}

Z_int BitVector_Sign(wordptr addr)
{
    N_word  size = size_(addr);
    N_word  mask = mask_(addr);
    wordptr last = addr + size - 1;
    boolean r    = true;

    if (size > 0)
    {
        *last &= mask;
        while (r and (size-- > 0)) r = ( *addr++ == 0 );
    }
    if (r) return((Z_int) 0);
    else
    {
        if (*last AND (mask AND NOT (mask >> 1))) return((Z_int) -1);
        else                                      return((Z_int)  1);
    }
}

ErrCode BitVector_Mul_Pos(wordptr X, wordptr Y, wordptr Z)
{
    Z_long  last;
    N_word  limit;
    N_word  count;
    boolean ok = true;

    /*
       Requirements:
         -  X and Y must have equal sizes (whereas Z may be any size!)
         -  Z should always contain the SMALLER of the two factors Y and Z
       Constraints:
         -  The contents of Y (and of X, of course) are destroyed
            (only Z is preserved!)
    */

    if (bits_(X) != bits_(Y)) return(ErrCode_Size);
    BitVector_Empty(X);
    if (BitVector_is_empty(Y)) return(ErrCode_Ok);
    if ((last = Set_Max(Z)) < 0L) return(ErrCode_Ok);
    limit = (N_word) last;
    for ( count = 0; (ok and (count <= limit)); count++ )
    {
        if ( BIT_VECTOR_TST_BIT(Z,count) )
            ok = not BitVector_add(X,X,Y,0);
        if (ok and (count < limit))
            ok = not BitVector_shift_left(Y,0);
    }
    if (ok) return(ErrCode_Ok); else return(ErrCode_Ovfl);
}

ErrCode BitVector_Multiply(wordptr X, wordptr Y, wordptr Z)
{
    ErrCode error = ErrCode_Ok;
    N_word  bit_x = bits_(X);
    N_word  bit_y = bits_(Y);
    N_word  bit_z = bits_(Z);
    N_word  size;
    N_word  mask;
    N_word  msb;
    wordptr ptr_x;
    wordptr ptr_y;
    wordptr ptr_z;
    boolean sgn_x;
    boolean sgn_y;
    boolean sgn_z;
    boolean zero;
    wordptr A;
    wordptr B;

    /*
       Requirements:
         -  Y and Z must have equal sizes
         -  X must have at least the same size as Y and Z but may be larger (!)
       Features:
         -  The contents of Y and Z are preserved
         -  X may be identical with Y or Z (or both!)
            (in-place multiplication is possible!)
    */

    if ((bit_y != bit_z) or (bit_x < bit_y)) return(ErrCode_Size);
    if (BitVector_is_empty(Y) or BitVector_is_empty(Z))
    {
        BitVector_Empty(X);
    }
    else
    {
        A = BitVector_Create(bit_y,false);
        if (A == NULL) return(ErrCode_Null);
        B = BitVector_Create(bit_z,false);
        if (B == NULL) { BitVector_Destroy(A); return(ErrCode_Null); }
        size  = size_(Y);
        mask  = mask_(Y);
        msb   = (mask AND NOT (mask >> 1));
        sgn_y = (((*(Y+size-1) &= mask) AND msb) != 0);
        sgn_z = (((*(Z+size-1) &= mask) AND msb) != 0);
        sgn_x = sgn_y XOR sgn_z;
        if (sgn_y) BitVector_Negate(A,Y); else BitVector_Copy(A,Y);
        if (sgn_z) BitVector_Negate(B,Z); else BitVector_Copy(B,Z);
        ptr_y = A + size;
        ptr_z = B + size;
        zero = true;
        while (zero and (size-- > 0))
        {
            zero &= (*(--ptr_y) == 0);
            zero &= (*(--ptr_z) == 0);
        }
        mask  = mask_(X);
        msb   = (mask AND NOT (mask >> 1));
        ptr_x = X + size_(X) - 1;
        if (*ptr_y > *ptr_z)
        {
            if (bit_x > bit_y)
            {
                A = BitVector_Resize(A,bit_x);
                if (A == NULL) { BitVector_Destroy(B); return(ErrCode_Null); }
            }
            if ((not (error = BitVector_Mul_Pos(X,A,B))) and
                ((*ptr_x AND msb) != 0)) error = ErrCode_Ovfl;
        }
        else
        {
            if (bit_x > bit_z)
            {
                B = BitVector_Resize(B,bit_x);
                if (B == NULL) { BitVector_Destroy(A); return(ErrCode_Null); }
            }
            if ((not (error = BitVector_Mul_Pos(X,B,A))) and
                ((*ptr_x AND msb) != 0)) error = ErrCode_Ovfl;
        }
        if ((not error) and sgn_x) BitVector_Negate(X,X);
        BitVector_Destroy(A);
        BitVector_Destroy(B);
    }
    return(error);
}

ErrCode BitVector_Div_Pos(wordptr Q, wordptr X, wordptr Y, wordptr R)
{
    N_word  bits = bits_(Q);
    N_word  mask;
    wordptr addr;
    Z_long  last;
    boolean flag;
    boolean copy = false; /* flags whether valid rest is in R (0) or X (1) */

    /*
       Requirements:
         -  All bit vectors must have equal sizes
         -  Q, X, Y and R must all be distinct bit vectors
         -  Y must be non-zero (of course!)
       Constraints:
         -  The contents of X (and Q and R, of course) are destroyed
            (only Y is preserved!)
    */

    if ((bits != bits_(X)) or (bits != bits_(Y)) or (bits != bits_(R)))
        return(ErrCode_Size);
    if ((Q == X) or (Q == Y) or (Q == R) or (X == Y) or (X == R) or (Y == R))
        return(ErrCode_Same);
    if (BitVector_is_empty(Y))
        return(ErrCode_Zero);

    BitVector_Empty(R);
    BitVector_Copy(Q,X);
    if ((last = Set_Max(Q)) < 0L) return(ErrCode_Ok);
    bits = (N_word) ++last;
    while (bits-- > 0)
    {
        addr = Q + (bits >> LOGBITS);
        mask = BITMASKTAB[bits AND MODMASK];
        flag = ((*addr AND mask) != 0);
        if (copy)
        {
            BitVector_shift_left(X,flag);
            flag = BitVector_subtract(R,X,Y,0);
        }
        else
        {
            BitVector_shift_left(R,flag);
            flag = BitVector_subtract(X,R,Y,0);
        }
        if (flag) *addr &= NOT mask;
        else
        {
            *addr |= mask;
            copy = not copy;
        }
    }
    if (copy) BitVector_Copy(R,X);
    return(ErrCode_Ok);
}

ErrCode BitVector_Divide(wordptr Q, wordptr X, wordptr Y, wordptr R)
{
    ErrCode error = ErrCode_Ok;
    N_word  bits = bits_(Q);
    N_word  size = size_(Q);
    N_word  mask = mask_(Q);
    N_word  msb = (mask AND NOT (mask >> 1));
    boolean sgn_q;
    boolean sgn_x;
    boolean sgn_y;
    wordptr A;
    wordptr B;

    /*
       Requirements:
         -  All bit vectors must have equal sizes
         -  Q and R must be two distinct bit vectors
         -  Y must be non-zero (of course!)
       Features:
         -  The contents of X and Y are preserved
         -  Q may be identical with X or Y (or both)
            (in-place division is possible!)
         -  R may be identical with X or Y (or both)
            (but not identical with Q!)
    */

    if ((bits != bits_(X)) or (bits != bits_(Y)) or (bits != bits_(R)))
        return(ErrCode_Size);
    if (Q == R)
        return(ErrCode_Same);
    if (BitVector_is_empty(Y))
        return(ErrCode_Zero);

    if (BitVector_is_empty(X))
    {
        BitVector_Empty(Q);
        BitVector_Empty(R);
    }
    else
    {
        A = BitVector_Create(bits,false);
        if (A == NULL) return(ErrCode_Null);
        B = BitVector_Create(bits,false);
        if (B == NULL) { BitVector_Destroy(A); return(ErrCode_Null); }
        size--;
        sgn_x = (((*(X+size) &= mask) AND msb) != 0);
        sgn_y = (((*(Y+size) &= mask) AND msb) != 0);
        sgn_q = sgn_x XOR sgn_y;
        if (sgn_x) BitVector_Negate(A,X); else BitVector_Copy(A,X);
        if (sgn_y) BitVector_Negate(B,Y); else BitVector_Copy(B,Y);
        if (not (error = BitVector_Div_Pos(Q,A,B,R)))
        {
            if (sgn_q) BitVector_Negate(Q,Q);
            if (sgn_x) BitVector_Negate(R,R);
        }
        BitVector_Destroy(A);
        BitVector_Destroy(B);
    }
    return(error);
}

ErrCode BitVector_GCD(wordptr X, wordptr Y, wordptr Z)
{
    ErrCode error = ErrCode_Ok;
    N_word  bits = bits_(X);
    N_word  size = size_(X);
    N_word  mask = mask_(X);
    N_word  msb = (mask AND NOT (mask >> 1));
    wordptr Q;
    wordptr R;
    wordptr A;
    wordptr B;
    wordptr T;

    /*
       Requirements:
         -  All bit vectors must have equal sizes
         -  Y and Z must be non-zero (of course!)
       Features:
         -  The contents of Y and Z are preserved
         -  X may be identical with Y or Z (or both)
            (in-place is possible!)
    */

    if ((bits != bits_(Y)) or (bits != bits_(Z))) return(ErrCode_Size);
    if (BitVector_is_empty(Y) or BitVector_is_empty(Z)) return(ErrCode_Zero);

    Q = BitVector_Create(bits,false);
    if (Q == NULL)
    {
        return(ErrCode_Null);
    }
    R = BitVector_Create(bits,false);
    if (R == NULL)
    {
        BitVector_Destroy(Q);
        return(ErrCode_Null);
    }
    A = BitVector_Create(bits,false);
    if (A == NULL)
    {
        BitVector_Destroy(Q);
        BitVector_Destroy(R);
        return(ErrCode_Null);
    }
    B = BitVector_Create(bits,false);
    if (B == NULL)
    {
        BitVector_Destroy(Q);
        BitVector_Destroy(R);
        BitVector_Destroy(A);
        return(ErrCode_Null);
    }
    size--;
    if (((*(Y+size) &= mask) AND msb) != 0) BitVector_Negate(A,Y);
    else                                    BitVector_Copy(A,Y);
    if (((*(Z+size) &= mask) AND msb) != 0) BitVector_Negate(B,Z);
    else                                    BitVector_Copy(B,Z);
    while (not error)
    {
        if (not (error = BitVector_Div_Pos(Q,A,B,R)))
        {
            if (BitVector_is_empty(R)) break;
            T = A;
            A = B;
            B = R;
            R = T;
        }
    }
    if (not error) BitVector_Copy(X,B);
    BitVector_Destroy(Q);
    BitVector_Destroy(R);
    BitVector_Destroy(A);
    BitVector_Destroy(B);
    return(error);
}

ErrCode BitVector_Power(wordptr X, wordptr Y, wordptr Z)
{
    ErrCode error = ErrCode_Ok;
    N_word  bits  = bits_(X);
    boolean first = true;
    Z_long  last;
    N_word  limit;
    N_word  count;
    wordptr T;

    /*
       Requirements:
         -  X must have at least the same size as Y but may be larger (!)
         -  X may not be identical with Z
         -  Z must be positive
       Features:
         -  The contents of Y and Z are preserved
    */

    if (X == Z) return(ErrCode_Same);
    if (bits < bits_(Y)) return(ErrCode_Size);
    if (BitVector_msb(Z)) return(ErrCode_Expo);
    if ((last = Set_Max(Z)) < 0L)
    {
        if (bits < 2) return(ErrCode_Ovfl);
        BitVector_Empty(X);
        *X |= LSB;
        return(ErrCode_Ok);                             /* anything ^ 0 == 1 */
    }
    if (BitVector_is_empty(Y))
    {
        if (X != Y) BitVector_Empty(X);
        return(ErrCode_Ok);                    /* 0 ^ anything not zero == 0 */
    }
    T = BitVector_Create(bits,false);
    if (T == NULL) return(ErrCode_Null);
    limit = (N_word) last;
    for ( count = 0; ((!error) and (count <= limit)); count++ )
    {
        if ( BIT_VECTOR_TST_BIT(Z,count) )
        {
            if (first)
            {
                first = false;
                if (count) {             BitVector_Copy(X,T); }
                else       { if (X != Y) BitVector_Copy(X,Y); }
            }
            else error = BitVector_Multiply(X,T,X); /* order important because T > X */
        }
        if ((!error) and (count < limit))
        {
            if (count) error = BitVector_Multiply(T,T,T);
            else       error = BitVector_Multiply(T,Y,Y);
        }
    }
    BitVector_Destroy(T);
    return(error);
}

void BitVector_Block_Store(wordptr addr, charptr buffer, N_int length)
{
    N_word  size = size_(addr);
    N_word  mask = mask_(addr);
    N_word  value;
    N_word  count;

    /* provide translation for independence of endian-ness: */
    if (size > 0)
    {
        while (size-- > 0)
        {
            value = 0;
            for ( count = 0; (length > 0) and (count < BITS); count += 8 )
            {
                value |= (((N_word) *buffer++) << count); length--;
            }
            *addr++ = value;
        }
        *(--addr) &= mask;
    }
}

charptr BitVector_Block_Read(wordptr addr, N_intptr length)
{
    N_word  size = size_(addr);
    N_word  value;
    N_word  count;
    charptr buffer;
    charptr target;

    /* provide translation for independence of endian-ness: */
    *length = size << FACTOR;
    buffer = (charptr) malloc((size_t) ((*length)+1));
    if (buffer == NULL) return(NULL);
    target = buffer;
    if (size > 0)
    {
        *(addr+size-1) &= mask_(addr);
        while (size-- > 0)
        {
            value = *addr++;
            count = BITS >> 3;
            while (count-- > 0)
            {
                *target++ = (N_char) (value AND 0x00FF);
                if (count > 0) value >>= 8;
            }
        }
    }
    *target = (N_char) '\0';
    return(buffer);
}

void BitVector_Word_Store(wordptr addr, N_int offset, N_int value)
{
    N_word size = size_(addr);

    if (size > 0)
    {
        if (offset < size) *(addr+offset) = value;
        *(addr+size-1) &= mask_(addr);
    }
}

N_int BitVector_Word_Read(wordptr addr, N_int offset)
{
    N_word size = size_(addr);

    if (size > 0)
    {
        *(addr+size-1) &= mask_(addr);
        if (offset < size) return( *(addr+offset) );
    }
    return( (N_int) 0 );
}

void BitVector_Word_Insert(wordptr addr, N_int offset, N_int count,
                           boolean clear)
{
    N_word  size = size_(addr);
    N_word  mask = mask_(addr);
    wordptr last = addr+size-1;

    if (size > 0)
    {
        *last &= mask;
        if (offset > size) offset = size;
        BIT_VECTOR_ins_words(addr+offset,size-offset,count,clear);
        *last &= mask;
    }
}

void BitVector_Word_Delete(wordptr addr, N_int offset, N_int count,
                           boolean clear)
{
    N_word  size = size_(addr);
    N_word  mask = mask_(addr);
    wordptr last = addr+size-1;

    if (size > 0)
    {
        *last &= mask;
        if (offset > size) offset = size;
        BIT_VECTOR_del_words(addr+offset,size-offset,count,clear);
        *last &= mask;
    }
}

void BitVector_Chunk_Store(wordptr addr, N_int chunksize, N_int offset,
                           N_long value)
{
    N_word bits = bits_(addr);
    N_word mask;
    N_word temp;

    if ((chunksize > 0) and (offset < bits))
    {
        if (chunksize > LONGBITS) chunksize = LONGBITS;
        if ((offset + chunksize) > bits) chunksize = bits - offset;
        addr += offset >> LOGBITS;
        offset &= MODMASK;
        while (chunksize > 0)
        {
            mask = (N_word) (~0L << offset);
            bits = offset + chunksize;
            if (bits < BITS)
            {
                mask &= (N_word) ~(~0L << bits);
                bits = chunksize;
            }
            else bits = BITS - offset;
            temp = (N_word) (value << offset);
            temp &= mask;
            *addr &= NOT mask;
            *addr++ |= temp;
            value >>= bits;
            chunksize -= bits;
            offset = 0;
        }
    }
}

N_long BitVector_Chunk_Read(wordptr addr, N_int chunksize, N_int offset)
{
    N_word bits = bits_(addr);
    N_word chunkbits = 0;
    N_long value = 0L;
    N_long temp;
    N_word mask;

    if ((chunksize > 0) and (offset < bits))
    {
        if (chunksize > LONGBITS) chunksize = LONGBITS;
        if ((offset + chunksize) > bits) chunksize = bits - offset;
        addr += offset >> LOGBITS;
        offset &= MODMASK;
        while (chunksize > 0)
        {
            bits = offset + chunksize;
            if (bits < BITS)
            {
                mask = (N_word) ~(~0L << bits);
                bits = chunksize;
            }
            else
            {
                mask = (N_word) ~0L;
                bits = BITS - offset;
            }
            temp = (N_long) ((*addr++ AND mask) >> offset);
            value |= temp << chunkbits;
            chunkbits += bits;
            chunksize -= bits;
            offset = 0;
        }
    }
    return(value);
}

    /*******************/
    /* set operations: */
    /*******************/

void Set_Union(wordptr X, wordptr Y, wordptr Z)             /* X = Y + Z     */
{
    N_word bits = bits_(X);
    N_word size = size_(X);
    N_word mask = mask_(X);

    if ((size > 0) and (bits == bits_(Y)) and (bits == bits_(Z)))
    {
        while (size-- > 0) *X++ = *Y++ OR *Z++;
        *(--X) &= mask;
    }
}

void Set_Intersection(wordptr X, wordptr Y, wordptr Z)      /* X = Y * Z     */
{
    N_word bits = bits_(X);
    N_word size = size_(X);
    N_word mask = mask_(X);

    if ((size > 0) and (bits == bits_(Y)) and (bits == bits_(Z)))
    {
        while (size-- > 0) *X++ = *Y++ AND *Z++;
        *(--X) &= mask;
    }
}

void Set_Difference(wordptr X, wordptr Y, wordptr Z)        /* X = Y \ Z     */
{
    N_word bits = bits_(X);
    N_word size = size_(X);
    N_word mask = mask_(X);

    if ((size > 0) and (bits == bits_(Y)) and (bits == bits_(Z)))
    {
        while (size-- > 0) *X++ = *Y++ AND NOT *Z++;
        *(--X) &= mask;
    }
}

void Set_ExclusiveOr(wordptr X, wordptr Y, wordptr Z)       /* X=(Y+Z)\(Y*Z) */
{
    N_word bits = bits_(X);
    N_word size = size_(X);
    N_word mask = mask_(X);

    if ((size > 0) and (bits == bits_(Y)) and (bits == bits_(Z)))
    {
        while (size-- > 0) *X++ = *Y++ XOR *Z++;
        *(--X) &= mask;
    }
}

void Set_Complement(wordptr X, wordptr Y)                   /* X = ~Y        */
{
    N_word size = size_(X);
    N_word mask = mask_(X);

    if ((size > 0) and (bits_(X) == bits_(Y)))
    {
        while (size-- > 0) *X++ = NOT *Y++;
        *(--X) &= mask;
    }
}

    /******************/
    /* set functions: */
    /******************/

boolean Set_subset(wordptr X, wordptr Y)                    /* X subset Y ?  */
{
    N_word size = size_(X);
    boolean r = false;

    if ((size > 0) and (bits_(X) == bits_(Y)))
    {
        r = true;
        while (r and (size-- > 0)) r = ((*X++ AND NOT *Y++) == 0);
    }
    return(r);
}

N_int Set_Norm(wordptr addr)                                /* = | X |       */
{
    N_word  size  = size_(addr);
    N_int   count = 0;
    N_word  c;

    while (size-- > 0)
    {
        c = *addr++;
        while (c)
        {
            c &= c - 1;
            count++;
        }
    }
    return(count);
}

Z_long Set_Min(wordptr addr)                                /* = min(X)      */
{
    boolean empty = true;
    N_word  size  = size_(addr);
    N_word  i     = 0;
    N_word  c;

    while (empty and (size-- > 0))
    {
        if (c = *addr++) empty = false; else i++;
    }
    if (empty) return((Z_long) LONG_MAX);                  /* plus infinity  */
    i <<= LOGBITS;
    while (not (c AND LSB))
    {
        c >>= 1;
        i++;
    }
    return((Z_long) i);
}

Z_long Set_Max(wordptr addr)                                /* = max(X)      */
{
    boolean empty = true;
    N_word  size  = size_(addr);
    N_word  i     = size;
    N_word  c;

    addr += size-1;
    while (empty and (size-- > 0))
    {
        if (c = *addr--) empty = false; else i--;
    }
    if (empty) return((Z_long) LONG_MIN);                  /* minus infinity */
    i <<= LOGBITS;
    while (not (c AND MSB))
    {
        c <<= 1;
        i--;
    }
    return((Z_long) --i);
}

    /**********************************/
    /* matrix-of-booleans operations: */
    /**********************************/

void Matrix_Multiplication(wordptr X, N_int rowsX, N_int colsX,
                           wordptr Y, N_int rowsY, N_int colsY,
                           wordptr Z, N_int rowsZ, N_int colsZ)
{
    N_word i;
    N_word j;
    N_word k;
    N_word indxX;
    N_word indxY;
    N_word indxZ;
    N_word termX;
    N_word termY;
    N_word sum;

  if ((colsY == rowsZ) and (rowsX == rowsY) and (colsX == colsZ) and
      (bits_(X) == rowsX*colsX) and
      (bits_(Y) == rowsY*colsY) and
      (bits_(Z) == rowsZ*colsZ))
  {
    for ( i = 0; i < rowsY; i++ )
    {
        termX = i * colsX;
        termY = i * colsY;
        for ( j = 0; j < colsZ; j++ )
        {
            indxX = termX + j;
            sum = 0;
            for ( k = 0; k < colsY; k++ )
            {
                indxY = termY + k;
                indxZ = k * colsZ + j;
                if ( BIT_VECTOR_TST_BIT(Y,indxY) &&
                     BIT_VECTOR_TST_BIT(Z,indxZ) ) sum ^= 1;
            }
            if (sum) BIT_VECTOR_SET_BIT(X,indxX)
            else     BIT_VECTOR_CLR_BIT(X,indxX)
        }
    }
  }
}

void Matrix_Product(wordptr X, N_int rowsX, N_int colsX,
                    wordptr Y, N_int rowsY, N_int colsY,
                    wordptr Z, N_int rowsZ, N_int colsZ)
{
    N_word i;
    N_word j;
    N_word k;
    N_word indxX;
    N_word indxY;
    N_word indxZ;
    N_word termX;
    N_word termY;
    N_word sum;

  if ((colsY == rowsZ) and (rowsX == rowsY) and (colsX == colsZ) and
      (bits_(X) == rowsX*colsX) and
      (bits_(Y) == rowsY*colsY) and
      (bits_(Z) == rowsZ*colsZ))
  {
    for ( i = 0; i < rowsY; i++ )
    {
        termX = i * colsX;
        termY = i * colsY;
        for ( j = 0; j < colsZ; j++ )
        {
            indxX = termX + j;
            sum = 0;
            for ( k = 0; k < colsY; k++ )
            {
                indxY = termY + k;
                indxZ = k * colsZ + j;
                if ( BIT_VECTOR_TST_BIT(Y,indxY) &&
                     BIT_VECTOR_TST_BIT(Z,indxZ) ) sum |= 1;
            }
            if (sum) BIT_VECTOR_SET_BIT(X,indxX)
            else     BIT_VECTOR_CLR_BIT(X,indxX)
        }
    }
  }
}

void Matrix_Closure(wordptr addr, N_int rows, N_int cols)
{
    N_word i;
    N_word j;
    N_word k;
    N_word ii;
    N_word ij;
    N_word ik;
    N_word kj;
    N_word termi;
    N_word termk;

  if ((rows == cols) and (bits_(addr) == rows*cols))
  {
    for ( i = 0; i < rows; i++ )
    {
        ii = i * cols + i;
        BIT_VECTOR_SET_BIT(addr,ii)
    }
    for ( k = 0; k < rows; k++ )
    {
        termk = k * cols;
        for ( i = 0; i < rows; i++ )
        {
            termi = i * cols;
            ik = termi + k;
            for ( j = 0; j < rows; j++ )
            {
                ij = termi + j;
                kj = termk + j;
                if ( BIT_VECTOR_TST_BIT(addr,ik) &&
                     BIT_VECTOR_TST_BIT(addr,kj) )
                     BIT_VECTOR_SET_BIT(addr,ij)
            }
        }
    }
  }
}

void Matrix_Transpose(wordptr X, N_int rowsX, N_int colsX,
                      wordptr Y, N_int rowsY, N_int colsY)
{
    N_word  i;
    N_word  j;
    N_word  ii;
    N_word  ij;
    N_word  ji;
    N_word  addii;
    N_word  addij;
    N_word  addji;
    N_word  bitii;
    N_word  bitij;
    N_word  bitji;
    N_word  termi;
    N_word  termj;
    boolean swap;

  /* BEWARE that "in-place" is ONLY possible if the matrix is quadratic!! */

  if ((rowsX == colsY) and (colsX == rowsY) and
      (bits_(X) == rowsX*colsX) and
      (bits_(Y) == rowsY*colsY))
  {
    if (rowsY == colsY) /* in-place is possible! */
    {
        for ( i = 0; i < rowsY; i++ )
        {
            termi = i * colsY;
            for ( j = 0; j < i; j++ )
            {
                termj = j * colsX;
                ij = termi + j;
                ji = termj + i;
                addij = ij >> LOGBITS;
                addji = ji >> LOGBITS;
                bitij = BITMASKTAB[ij AND MODMASK];
                bitji = BITMASKTAB[ji AND MODMASK];
                swap = ((*(Y+addij) AND bitij) != 0);
                if ((*(Y+addji) AND bitji) != 0)
                     *(X+addij) |=     bitij;
                else
                     *(X+addij) &= NOT bitij;
                if (swap)
                     *(X+addji) |=     bitji;
                else
                     *(X+addji) &= NOT bitji;
            }
            ii = termi + i;
            addii = ii >> LOGBITS;
            bitii = BITMASKTAB[ii AND MODMASK];
            if ((*(Y+addii) AND bitii) != 0)
                 *(X+addii) |=     bitii;
            else
                 *(X+addii) &= NOT bitii;
        }
    }
    else /* rowsX != colsX, in-place is NOT possible! */
    {
        for ( i = 0; i < rowsY; i++ )
        {
            termi = i * colsY;
            for ( j = 0; j < colsY; j++ )
            {
                termj = j * colsX;
                ij = termi + j;
                ji = termj + i;
                addij = ij >> LOGBITS;
                addji = ji >> LOGBITS;
                bitij = BITMASKTAB[ij AND MODMASK];
                bitji = BITMASKTAB[ji AND MODMASK];
                if ((*(Y+addij) AND bitij) != 0)
                     *(X+addji) |=     bitji;
                else
                     *(X+addji) &= NOT bitji;
            }
        }
    }
  }
}

/*****************************************************************************/
/*  VERSION:  5.8                                                            */
/*****************************************************************************/
/*  VERSION HISTORY:                                                         */
/*****************************************************************************/
/*                                                                           */
/*    Version 5.8  14.07.00  Added "Power()". Changed "Copy()".              */
/*    Version 5.7  19.05.99  Quickened "Div_Pos()". Added "Product()".       */
/*    Version 5.6  02.11.98  Leading zeros eliminated in "to_Hex()".         */
/*    Version 5.5  21.09.98  Fixed bug of uninitialized "error" in Multiply. */
/*    Version 5.4  07.09.98  Fixed bug of uninitialized "error" in Divide.   */
/*    Version 5.3  12.05.98  Improved Norm. Completed history.               */
/*    Version 5.2  31.03.98  Improved Norm.                                  */
/*    Version 5.1  09.03.98  No changes.                                     */
/*    Version 5.0  01.03.98  Major additions and rewrite.                    */
/*    Version 4.2  16.07.97  Added is_empty, is_full.                        */
/*    Version 4.1  30.06.97  Added word-ins/del, move-left/right, inc/dec.   */
/*    Version 4.0  23.04.97  Rewrite. Added bit shift and bool. matrix ops.  */
/*    Version 3.2  04.02.97  Added interval methods.                         */
/*    Version 3.1  21.01.97  Fixed bug on 64 bit machines.                   */
/*    Version 3.0  12.01.97  Added flip.                                     */
/*    Version 2.0  14.12.96  Efficiency and consistency improvements.        */
/*    Version 1.1  08.01.96  Added Resize and ExclusiveOr.                   */
/*    Version 1.0  14.12.95  First version under UNIX (with Perl module).    */
/*    Version 0.9  01.11.93  First version of C library under MS-DOS.        */
/*    Version 0.1  ??.??.89  First version in Turbo Pascal under CP/M.       */
/*                                                                           */
/*****************************************************************************/
/*  AUTHOR:                                                                  */
/*****************************************************************************/
/*                                                                           */
/*    Steffen Beyer                                                          */
/*    Ainmillerstr. 5 / App. 513                                             */
/*    D-80801 Munich                                                         */
/*    Germany                                                                */
/*                                                                           */
/*    mailto:sb@engelschall.com                                              */
/*    http://www.engelschall.com/u/sb/download/                              */
/*                                                                           */
/*****************************************************************************/
/*  COPYRIGHT:                                                               */
/*****************************************************************************/
/*                                                                           */
/*    Copyright (c) 1995 - 2000 by Steffen Beyer.                            */
/*    All rights reserved.                                                   */
/*                                                                           */
/*****************************************************************************/
/*  LICENSE:                                                                 */
/*****************************************************************************/
/*                                                                           */
/*    This library is free software; you can redistribute it and/or          */
/*    modify it under the terms of the GNU Library General Public            */
/*    License as published by the Free Software Foundation; either           */
/*    version 2 of the License, or (at your option) any later version.       */
/*                                                                           */
/*    This library is distributed in the hope that it will be useful,        */
/*    but WITHOUT ANY WARRANTY; without even the implied warranty of         */
/*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       */
/*    Library General Public License for more details.                       */
/*                                                                           */
/*    You should have received a copy of the GNU Library General Public      */
/*    License along with this library; if not, write to the                  */
/*    Free Software Foundation, Inc.,                                        */
/*    59 Temple Place, Suite 330, Boston, MA 02111-1307 USA                  */
/*                                                                           */
/*    or download a copy from ftp://ftp.gnu.org/pub/gnu/COPYING.LIB-2.0      */
/*                                                                           */
/*****************************************************************************/
#endif
