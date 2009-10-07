/*
  Hatari - calculate.c

  Copyright (C) 1994, 2009 by Eero Tamminen

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  calculate.c - largely modified version of the Clac calculator
  filter version code to calculate expressions for Hatari.
*/
const char Clac_fileid[] = "Hatari clac.c : " __DATE__ " " __TIME__;
/* ====================================================================	*/
/*			*** Clac engine ***				*/
/* ====================================================================	*/

#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include "calculate.h"

/* define which character indicates which type of number on expression  */
#define BIN_SYM '%'                            /* binary decimal       */
#define DEC_SYM '#'                             /* normal decimal       */
#define HEX_SYM '$'                             /* hexadecimal          */

/* define error codes                                                   */
#define CLAC_EXP_ERR "No expression given"
#define CLAC_GEN_ERR "Syntax error"
#define CLAC_PAR_ERR "Mismatched parenthesis"
#define CLAC_DEF_ERR "Undefined result (1/0)"
#define CLAC_STK_ERR "Operation/value stack full"
#define CLAC_OVF_ERR "Overflow"
#define CLAC_OVR_ERR "Mode overflow"
#define CLAC_PRG_ERR "Internal program error"

/* define internal allocation sizes (should be enough ;-)		*/
#define PARDEPTH_MAX	64		/* max. parenth. nesting depth	*/
#define OSTACK_MAX	128		/* size of the operator stack	*/
#define VSTACK_MAX	128		/* size of the value stack	*/

/* operation with lowest precedence, used to finish calculations */
#define LOWEST_PREDECENCE '|'

/* globals + function identifier stack(s)				*/
static struct {
	const char *error;		/* global error code		*/
	bool valid;			/* value validation		*/
} id = {0, 0};

/* parenthesis and function stacks					*/
static struct {
	int idx;			/* parenthesis level		*/
	int max;			/* maximum idx			*/
	int opx[PARDEPTH_MAX + 1];	/* current op index for par	*/
	int vax[PARDEPTH_MAX + 1];	/* current val index for par	*/
} par = {0, PARDEPTH_MAX, {0}, {0}};

static struct {					/* operator stack	*/
	int idx;
	int max;
	char buf[OSTACK_MAX + 1];
} op = {0, OSTACK_MAX, ""};

static struct value_stk {			/* value stack	*/
	int idx;
	int max;
	long long buf[VSTACK_MAX + 1];
} val = {0, VSTACK_MAX, {0}};

/* -------------------------------------------------------------------- */
/* macros								*/

/* increment stack index and put value on stack (ie. PUSH)		*/
#define PUSH(stk,val) \
	if((stk).idx < (stk).max) {		\
		(stk).idx += 1;			\
		(stk).buf[(stk).idx] = (val);	\
	} else {				\
		id.error = CLAC_STK_ERR;	\
	}

/* -------------------------------------------------------------------- */
/* declare subfunctions							*/

/* parse a decimal from an expr. */
static long long get_decimal(const char *expr, int *offset);
/* parse value */
static long long get_value(const char *expr, int *offset, int bits);
/* 'value' of a char */
static int chr_pos(char c, const char *base, int base_len);
/* parse in-between operations	*/
static void operation(long long value, char op);
/* parse unary operators	*/
static void unary (char op);
/* apply a prefix to a value */
static void apply_prefix(void);
/* juggle stacks, if possible	*/
static void eval_stack(void);
/* operator -> operator level	*/
static int get_level(int stk_offset);
/* evaluate operation		*/
static long long apply_op(char op, long long x, long long y);

/* increase parenthesis level	*/
static void open_bracket(void);
/* decrease parenthesis level	*/
static long long close_bracket(long long x);

/**
 * Evaluate expression.
 * Set given value and parsing offset, return error string or NULL for success.
 */
const char* calculate (const char *in, long long *out, int *erroff)
{
	/* in	 : expression to evaluate				*/
	/* out	 : final parsed value					*/
	/* value : current parsed value					*/
	/* mark	 : current character in expression			*/
	/* valid : expression validation flag, set when number parsed	*/
	/* end	 : 'expression end' flag				*/
	/* offset: character offset in expression			*/

	bool end = false;
	int offset = 0;
	long long value;
	char mark;
	
	/* Uses global variables:	*/

	par.idx = 0;			/* parenthesis stack pointer	*/
	par.opx[0] = par.vax[0] = 0;	/* additional stack pointers	*/
	op.idx = val.idx = -1;

	id.error = NULL;
	id.valid = false;		/* value validation		*/
	value = 0;

	/* parsing loop, repeated until expression ends */
	do {
		mark = in[offset];
		switch(mark) {
		case ' ':
		case '\t':
			offset ++;		/* jump over white space */
			break;
		case '~':			/* prefixes */
			unary(mark);
			offset ++;
			break;
		case '>':			/* operators  */
		case '<':
			offset ++;
			/* check that it's '>>' or '<<' */
			if (in[offset] != mark)
			{
				id.error = CLAC_GEN_ERR;
				break;
			}
			operation (value, mark);
			id.valid = false;
			offset ++;
			break;
		case '|':
		case '&':
		case '^':
		case '+':
		case '-':
		case '*':
		case '/':
			operation (value, mark);
			id.valid = false;
			offset ++;
			break;
		case '(':
			open_bracket ();
			offset ++;
			break;
		case ')':
			value = close_bracket (value);
			offset ++;
			break;
		case '0':
			/* C notation for hex, or normal decimals? */
			if(in[offset + 1] == 'x')
			{
				offset += 2;
				value = get_value(in, &offset, 4);
				break;
			}
		case '1':				/* decimal	*/
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case '.':
			value = get_decimal (in, &offset);
			break;
		case BIN_SYM:      /* binary decimal  */
			offset ++;
			value = get_value(in, &offset, 1);
			break;
		case DEC_SYM:      /* normal decimal prefix  */
			offset ++;
			value = get_decimal(in, &offset);
			break;
		case HEX_SYM:      /* hexadecimal    */
			offset ++;
			value = get_value(in, &offset, 4);
			break;
		default:
			/* end of expression or error... */
			if(mark < ' ' || mark == ';')
				end = true;
			else
				id.error = CLAC_GEN_ERR;
		}

	/* until exit or error message					*/
	} while(!(end || id.error));

        /* result of evaluation 					*/
        if (val.idx >= 0)
		*out = val.buf[val.idx];

	/* something to return?						*/
	if (!id.error) {
		if (id.valid) {

			/* evaluate rest of the expression		*/
			operation (value, LOWEST_PREDECENCE);
			if (par.idx)			/* mismatched	*/
				id.error = CLAC_PAR_ERR;
			else				/* result out	*/
				*out = val.buf[val.idx];

		} else {
			if ((val.idx < 0) && (op.idx < 0)) {
				id.error = CLAC_EXP_ERR;
				*out = 0;
			} else			/* trailing operators	*/
				id.error = CLAC_GEN_ERR;
		}
	}

	*erroff = offset;
	if (id.error) {
		return id.error;
	}
	return NULL;
}

/* ==================================================================== */
/*			parse a value					*/
/* ==================================================================== */

/**
 * parse a decimal number
 */
static long long get_decimal(const char *expr, int *offset)
{
	char mark;
	int mark_set = false, expr_set = false;
	long long value = 0;
	
	if(id.valid == false)
	{
		id.valid = true;
		value = atof(&expr[*offset]);
		/* jump over number */
		do
		{
			mark = expr[++(*offset)];
			/* check for multiple decimal points */
			if(mark == '.')
			{
				if(mark_set)
					id.error = CLAC_GEN_ERR;
				else
					mark_set = true;
			}
			/* check for multiple exponents */
			if(mark == 'e' || mark == 'E')
			{
				if(expr_set)
				{
					id.error = CLAC_GEN_ERR;
				}
				else
				{
					/* check for exponent validity */
					mark = expr[++(*offset)];
					if(mark == '+' || mark == '-' ||
					   (mark >= '0' && mark <= '9'))
					{
						mark_set = true;
						expr_set = true;
						mark = '.';
					}
					else
						id.error = CLAC_GEN_ERR;
				}
			}
		} while(!id.error &&
			((mark >= '0' && mark <= '9') || mark == '.'));
	}
	else
		id.error = CLAC_GEN_ERR;
	
	return value;
}

/**
 * parsing for 2^bits number base(up to hex, at the moment)
 */
static long long get_value(const char *expr, int *offset, int bits)
{
	/* returns parsed value, changes expression offset */

	long long value = 0;
	int i, end, lenny, pos, idx, len_long = sizeof(long) * 8;
	unsigned long num1 = 0, num2 = 0; /* int/decimal  parts  */
	char digit;
	const char *base = "0123456789ABCDEF"; /* number base(s)      */
	end = 1 << bits;                       /* end of current base */
	if(bits == 1) len_long --;             /* eliminate negate    */
	lenny = len_long / bits;               /* max. number lenght  */
	
	/* if start of expression or preceded by an operator */
	if(id.valid == false)
	{
		id.valid = true;
		i = 0;
		digit = expr[*offset];
		idx = chr_pos(digit, base, end);  /* digit value  */
		
		/* increment i until the integer part of value ends */
		while((i < lenny) && (idx >= 0))
		{
			num1 = (num1 << bits) | idx;
			digit = expr[++(*offset)];
			idx = chr_pos(digit, base, end);
			i ++;
		}
		/* too long number or expands into the sign bit?  */
		if(((i == lenny) && (idx >= 0)) ||
		   (num1 & (1L << (len_long - 1))))
			id.error = CLAC_OVR_ERR;
		else
		{
			/* decimal part? */
			if(digit == '.')
			{
				pos = len_long - bits;
				digit = expr[++(*offset)];
				idx = chr_pos(digit, base, end);
				
				/* calculate x / 0xFFFFFFFF */
				while((pos >= bits) && (idx >= 0))
				{
					pos -= bits;
					num2 |= (long)idx << pos;
					digit = expr[++(*offset)];
					idx = chr_pos(digit, base, end);
				}
				/* jump over any remaining decimals  */
				if((pos < bits) && (idx >= 0))
					while(chr_pos(expr[*offset], base, end) >= 0)
						(*offset) ++;
			}
			/* compose value of integral and decimal parts  */
			value = num2 / (long long) (1L << (len_long - bits)) + num1;
		}
	}
	else
		id.error = CLAC_GEN_ERR;
	
	return value;
}

/* -------------------------------------------------------------------- */
/**
 * return the value of a given character in the current number base
 */
static int chr_pos(char chr, const char *string, int len)
{
	/* returns a character position (0 - ) in a string or -1  */
	int pos = 0;        /* character position */
	chr = toupper(chr); /* uppercase a char   */
	
	/* till strings end or character found */
	while(pos < len && *(string ++) != chr)
		pos ++;
	
	/* if string end -> not found */
	if(pos == len)
		return -1;
	else
		return pos;
}

/* ==================================================================== */
/*			expression evaluation				*/
/* ==================================================================== */

static void operation (long long value, char oper)
{
	/* uses globals par[], id.error[], op[], val[]
	 * operation executed if the next one is on same or lower level
	 */
	/* something to calc? */
	if(id.valid == true) {
		/* next number */
		id.valid = false;
		
		/* add new items to stack */
		PUSH(op, oper);
		PUSH(val, value);
		
		/* more than 1 operator  */
		if(op.idx > par.opx[par.idx]) {

			/* but only one value */
			if(val.idx == par.vax[par.idx]) {
				apply_prefix();
			} else {
				/* evaluate all possible operations */
				eval_stack();
			}
		}
	} else {
		/* pre- or post-operators instead of in-betweens */
		unary(oper);
	}
}

/**
 * handle unary operators
 */
static void unary (char oper)
{
	/* check pre-value operators
	 * have to be parenthesised
	 */
	if(id.valid == false && op.idx < par.opx[par.idx])
	{
		switch(oper)
		{
		case '+':		/* not needed */
			break;
		case '-':
		case '~':
			PUSH(op, oper);
			break;
		default:
			id.error = CLAC_PRG_ERR;
		}
	}
	else
		id.error = CLAC_GEN_ERR;
}

/**
 * apply a prefix to the current value
 */
static void apply_prefix(void)
{
	long long value = val.buf[val.idx];

	op.idx--;
	switch(op.buf[op.idx])
	{
	case '-':
		value = (-value);
		break;
	case '~':
		value = (~value);
		break;
	default:
		id.error = CLAC_PRG_ERR;
	}
	val.buf[val.idx] = value;
	op.buf[op.idx] = op.buf[op.idx + 1];
}

/* -------------------------------------------------------------------- */
/**
 * evaluate operators if precedence allows it
 */
/* evaluate all possible (according to order of precedence) operators	*/
static void eval_stack (void)
{
	/* uses globals par[], op[], val[]	*/

	/* # of operators >= 2 and prev. op-level >= current op-level ?	*/
	while ((op.idx > par.opx[par.idx]) && get_level (-1) >= get_level (0)) {

		/* shorten value stacks by one	*/
		/* + calculate resulting value	*/
		op.idx -= 1;
		val.idx -= 1;
		val.buf[val.idx] = apply_op(op.buf[op.idx],
			val.buf[val.idx], val.buf[val.idx + 1]);

		/* pull the just used operator out of the stack		*/
		op.buf[op.idx] = op.buf[op.idx + 1];
	}
}

/* -------------------------------------------------------------------- */
/**
 * return the precedence level of a given operator
 */
static int get_level (int offset)
{
	/* used globals par[], op[]
	 * returns operator level of: operator[stack idx + offset]
	 */
	switch(op.buf[op.idx + offset]) {
	case '|':      /* binary operations  */
	case '&':
	case '^':
		return 0;
		
	case '>':      /* bit shifting    */
	case '<':
		return 1;
		
	case '+':
	case '-':
		return 2;
		
	case '*':
	case '/':
		return 3;
		
	default:
		id.error = CLAC_PRG_ERR;
	}
	return 6;
}

/* -------------------------------------------------------------------- */
/**
 * apply operator to given values, return the result
 */
static long long apply_op (char opcode, long long value1, long long value2)
{
	/* uses global id.error[]		*/
	/* returns the result of operation	*/

	switch (opcode) {
        case '|':
		value1 |= value2;
		break;
        case '&':
		value1 &= value2;
		break;
        case '^':
		value1 ^= value2;
		break;
        case '>':
		value1 >>= value2;
        case '<':
		value1 <<= value2;
		break;
	case '+':
		value1 += value2;
		break;
	case '-':
		value1 -= value2;
		break;
	case '*':
		value1 *= value2;
		break;
	case '/':
		/* don't divide by zero */
		if (value2)
			value1 /= value2;
		else
			id.error = CLAC_DEF_ERR;
		break;
        default:
		id.error = CLAC_PRG_ERR;
	}
	return value1;				/* return result	*/
}


/* ==================================================================== */
/*			parenthesis and help				*/
/* ==================================================================== */

/**
 * open prenthesis, push values & operators to stack
 */
static void open_bracket (void)
{
	if (id.valid == false) {		/* preceded by operator	*/
		if (par.idx < PARDEPTH_MAX) {	/* not nested too deep	*/
			par.idx ++;
			par.opx[par.idx] = op.idx + 1;
			par.vax[par.idx] = val.idx + 1;
		} else
			id.error = CLAC_STK_ERR;
	} else
		id.error = CLAC_GEN_ERR;
}

/* -------------------------------------------------------------------- */
/**
 * close prenthesis, and evaluate / pop stacks
 */
/* last parsed value, last param. flag, trigonometric mode	*/
static long long close_bracket (long long value)
{
	/* returns the value of the parenthesised expression	*/

	if (id.valid) {			/* preceded by an operator	*/
		if (par.idx > 0) {	/* prenthesis has a pair	*/
			/* calculate the value of parenthesised exp.	*/
			operation (value, LOWEST_PREDECENCE);
			value = val.buf[val.idx];
			op.idx = par.opx[par.idx] - 1;	/* restore prev	*/
			val.idx = par.vax[par.idx] - 1;
			par.idx --;

			/* next operator */
			id.valid = true;
		} else
			id.error = CLAC_PAR_ERR;
	} else
		id.error = CLAC_GEN_ERR;

	return value;
}