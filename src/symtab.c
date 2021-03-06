/***
 * $Source: /cvsroot-fuse/gump/FreeM/src/symtab.c,v $
 * $Revision: 1.6 $ $Date: 2000/02/22 17:48:24 $
 */
/*                            *
 *                           * *
 *                          *   *
 *                     ***************
 *                      * *       * *
 *                       *  MUMPS  *
 *                      * *       * *
 *                     ***************
 *                          *   *
 *                           * *
 *                            *
 *
 * Shalom ha-Ashkenaz, 1998/06/18 CE
 * 
 * mumps local symbol table and user defined special variable table
 * 
 */

#define ZNEW        'N'
#include "mpsdef.h"
char   *calloc ();
void    free ();

/* Turn this on to get tons of lovely debugging messages about
   symbol-table calls */
/* #define DEBUG_SYM  */

/* Use the new string structures */
#define NEW_WAY /* Do things the new way */
#define FOR 'f'
#ifdef NEW_WAY
#ifdef NEWSTACK
#define EXPERIMENTAL_NEW 
#endif
#define EXPERIMENTAL_SETGET  
#define NUMERIC /* Attempt to store data as a 'long' if possible */

/* Does the string have data? */
#define DATA(variable) (variable->data || (variable->rc->status & STAT_NUMERIC))

/* To-Do list
 * 01 Problem with *data: needs to be a short array to handle UNICODE
 * Solution: UNICODE can wait for now
 *
 * 02 It would be nice to have dynamic block sizes and discreet
 * blocks (i.e. one string structure would alloc as much as it
 * can, and only tack on another block when necessary)
 *
 * 03 The hash would work better if we used the LAST character
 * in the string as an index instead of the first one...
 * 
 * 04 Aliases/References need help
 */

/* Push this UP to decrease malloc() calls, improve speed 
   at the expense of memory (ST_BLOCK is the MINIMUM string size)
   push this DOWN to optimize memory usage at the expense of
   lots of calls, fragmented memory and a slight speed hit 
   a CHAR variable checks this, so ST_BLOCK should be <= 255 
   Which makes sense anyway, unless you have tons of memory,
   in which case you can go change all the char->int references
   yourself.  So there.  */
#define ST_BLOCK 255  /* Size of a string block */

/* A string structure (main symbol-table storage type) */
typedef struct _string {
  struct _string_resources *rc; /* Resource data */
  unsigned char datalen;	/* Length of data (ignored if *next) */

  /* data should be (short *) to use UNICODE */
  char *data;			/* The data (calloc'd ST_BLOCK) */
  struct _string *next_data;	/* Next chunk */
} string;

#define STAT_NUMERIC   0x01
#define STAT_REFERENCE 0x02

/* This contains string 'administration' fields */
typedef struct _string_resources {
  char *key;  		/* String keyname */
  long numeric;		/* Numeric value of string */
  char status;		/* Status byte */
  struct _string *reference;    /* If we're a reference, the original */
  struct _subscript *subs;	/* Subscripted? */
  struct _string *next_node;	/* Next string */
} string_rc;

/* A string-index, mini-hash for the first(?) letter of the key 
   minus the first 32 ASCII characters (invalid in keys) */
typedef struct _subscript { struct _string *index[96]; } subscript;
subscript *SymTable;  /* Main index */
subscript *UDFSV;     /* User-defined special variable index */

#ifdef NEWSTACK
typedef struct _string_stack_node {  /* Stack for NEWd variables */
  char *full_key;
  string *variable;
  struct _string_stack_node *next;
} st_stack_node;

typedef struct _sub_stack_node {     /* NEW-ALL stack */
  subscript *sub;
  struct _sub_stack_node *next;
} sub_stack_node;

/* This is used by $ORDER and $QUERY */
typedef struct _locator {
  struct _locator *parent; /* Next-higher branch */
  struct _string  *branch; /* This level */
  struct _locator *child;  /* Next-lower branch */
} locator;

/* This is used by $ZDATA */
typedef struct _counter {
  long datanodes;
  struct _counter *next;
} counter;

st_stack_node *string_stack = NULL; /* NEW variable stack */
sub_stack_node *sub_stack   = NULL; /* NEW-ALL stack */
extern stack_level *stack; /* stack */

/* These functions are used by $ORDER and $QUERY */
/* Same as seek_string, but creates a list showing all the branches
   it took to get to the string */
locator *seek_leaf(subscript *subs, char *key, locator *parent);
/* Returns either the first or last string with data in the
   subscript */
string *string_end(subscript *start, int first);
/* Returns the next or previous string on same level with data */
string *string_along(string *target, locator *parent, int next);
/* Returns the first string with data under a subscript:
   Descends further if necessary */
locator *get_first_string(subscript *start, string **get, locator *parent);
/* Same as get_first string, but with the last subscript */
locator *get_last_string (subscript *start, string **get, locator *parent);
/* Gets the last string with data in a linked-list */
string *get_last_datanode(string *start, string *stop);
/* Gets the string immediately previous on the same level, with data or not */
string *get_previous(string *target, locator *parent);
/* Same as get_previous for the next string */
string *get_next(string *target, locator *parent);

/* Used by ZDATA */
void count_subnodes(subscript *count, counter *this_level);
#endif

/* Function prototypes */

/* Find a place to store a variable, returns allocated space */
string *find_home(subscript *subs, char *key);

/* Looks for a variable, returns NULL if it can't find anything
   If given a ***last, sets it to the pointer to the returned
   variable (i.e., the last node or the subscript index )
 */
string *seek_string(subscript *subs, char *key, string ***last);

/* Destroy a string and all it's descendants */
int kill_string(string *fry);
/* Destroy a subscript and all strings indexed by it */
int kill_subscript(subscript *subs);

/* Copy one string to another, including all subscripts */
void copy_string(string *dest, string *src);
/* Copy one subscript to another, including all string descendants */
void copy_subscript(subscript *dest, subscript *src);
/* Merge two linked-lists of strings, like the ones hanging from
   subscripts */
string *copy_list(string *dest, string *src);

/* Is the character value a suitable numeric? Returns 1 or 0 */
int is_numeric(char *value);
#endif /* NEW WAY */

long str2long(char *string) {
  int loop=0; int mult = 1, exp = 1;
  long value = 0;
  if(string[0] == '-') { mult = -1; string++; }
  while(string[loop] != EOL && string[loop] >= '0' && string[loop] <= '9')
      loop++;
  loop--;
  while(loop > -1) {
      value += (string[loop] - '0') * exp;
      exp *= 10; loop--;
  }
  value *= mult;
  return value;
}

/* local symbol table management */

void
symtab (action, key, data)		/* symbol table functions */
				/* (+)functions are now re-implemented */
                                /* (!)functions are new */
                                /* (?)re-implemented, with issues */
	short   action;			/* +set_sym      +get_sym   */

					/* +kill_sym     +$data     */
					/* +kill_all     +$order    */
					/* +killexcl     +query     */
					/* +new_sym      +bigquery  */
					/* +new_all      +getinc    */
					/* +newexcl                 */
					/* +killone      +m_alias   */
					/* !merge_sym    +zdata     */
					/* !pop_sym		    */

	char   *key,			/* lvn as ASCII-string */
	       *data;

/* The symbol table is placed at the high end of 'partition'. It begins at
 * 'symlen' and ends at 'PSIZE'. The layout is
 * (keylength)(key...)(<EOL>)(datalength)(data...[<EOL>])
 * The keys are sorted in $order sequence.
 * 
 * ****possible future layout with less space requirements****
 * (keylength)(statusbyte)(key...)[(datalength)(data...[<EOL>])]
 * 'keylength' is the length of 'key' overhead bytes not included.
 * 'statusbyte' is an indicator with the following bits:
 * 0  (LSB)        1=data information is missing 0=there is a data field
 * 1               1=key is numeric              0=key is alphabetic
 * 2..7            0..number of previous key_pieces
 * note, that the status byte of a defined unsubscripted variable
 * is zero.
 * If a subscripted variable is stored, the variablename and each
 * subscript are separate entries in the symbol table.
 * E.g. S USA("CA",6789)="California" ; with $D(ABC)=0 before the set
 * then the following format is used:
 * (3)(    1)ABC
 * (2)(1*4+1)CA
 * (4)(2*4+2)6789(10)California
 * ****end of "possible future layout"****
 * To have the same fast access regardless of the position in the
 * alphabet for each character a pointer to the first variable beginning
 * with that letter is maintained. (0 indicates there's no such var.)
 */

{
/* must be static:                */
    static unsigned long tryfast = 0L;	/* last $order reference          */

/* the following variables may    */
/* be static or not               */
    static unsigned short nocompact = TRUE;	/* flag: do not compact symtab if */

/* value becomes shorter          */
/* be static or dynamic:          */
    static long keyl,			/* length of key                  */
            datal;			/* length of data                 */
    static long kill_from;
    static char tmp1[256], tmp2[256], tmp3[256];
    register long i, j, k, k1;
#ifdef DEBUG_SYM
    int i0, i1;
    char *start;
#endif
    int loop;
#ifdef EXPERIMENTAL_NEW
    int loop1, loop2;
    stack_level *tmp_stack;
    stack_cmd *new_command;
    st_stack_node *stack_node, *last_stack_node;
    sub_stack_node *sym_node, *last_sym_node;
#endif
#ifdef NUMERIC
    long long_tmp;
#endif
#ifdef EXPERIMENTAL_SETGET
    string *get, *free_string = NULL, *new_string, *tmp;
    string **ref_pointer; /* Pointer to the reference to the string! */
    char *data_copy; /* UNICODE me */
    char flag;
#ifdef NEWSTACK
    locator *loc_head, *loc_tmp, *loc_last, *loc_free;
    counter *counter_head, *counter_free;
#endif
#endif

    if(key) stcpy (zloc, key);
    if (v22ptr) {
	procv22 (key);
	if (key[0] == '^') {
	    char    zrsav[256];
	    int     naksav;
	    char    gosav[256];

	    stcpy (zrsav, zref);
	    naksav = nakoffs;
	    stcpy (gosav, g_o_val);
	    global  (action, key, data);

	    stcpy (zref, zrsav);
	    nakoffs = naksav;
	    stcpy (l_o_val, g_o_val);
	    stcpy (g_o_val, gosav);
	    return;
	}
    }
    if (glvnflag.all			/* process optional limitations */
	&& key[0] >= '%' && key[0] <= 'z') {
	if ((i = glvnflag.one[0])) {	/* number of significant chars */
	    j = 0;
	    while ((k1 = key[j]) != DELIM && k1 != EOL) {
		if (j >= i) {
		    while ((k1 = key[++j]) != DELIM && k1 != EOL) ;
		    stcpy (&key[i], &key[j]);
		    break;
		}
		j++;
	    }
	}
	if (glvnflag.one[1]) {		/* upper/lower sensitivity */
	    j = 0;
	    while ((k1 = key[j]) != DELIM && k1 != EOL) {
		if (k1 >= 'a' && k1 <= 'z')
		    key[j] = k1 - 32;
		j++;
	    }
	}
#ifndef EXPERIMENTAL_SETGET /* Don't check limits */
	if ((i = glvnflag.one[2])) {
	    if (stlen (key) > i) {
		ierr = MXSTR;
		return;
	    }				/* key length limit */
	}
	if ((i = glvnflag.one[3])) {	/* subscript length limit */
	    j = 0;
	    while ((k1 = key[j++]) != DELIM && k1 != EOL) ;
	    if (k1 == DELIM) {
		k = 0;
		for (;;)
		{
		    k1 = key[j++];
		    if (k1 == DELIM || k1 == EOL) {
			if (k > i) {
			    ierr = MXSTR;
			    return;
			}
			k = 0;
		    }
		    if (k1 == EOL) break;
		    k++;
		}
	    }
	}
#endif
    }
#ifndef NEWSTACK /* We don't do aliases this way anymore */
    if (aliases && (action != m_alias)) {	/* there are aliases */
	i = 0;
	j = 0;
	while (i < aliases) {
	    k1 = i + UNSIGN (ali[i]) + 1;
/* is current reference an alias ??? */
	    j = 0;
	    while (ali[++i] == key[j]) {
		if (ali[i] == EOL)
		    break;
		j++;
	    }
/* yes, it is, so resolve it now! */
	    if (ali[i] == EOL && (key[j] == EOL || key[j] == DELIM)) {
		stcpy (tmp1, key);
		stcpy (key, &ali[i + 1]);
		stcat (key, &tmp1[j]);
		i = 0;
		continue;		/* try again, it might be a double alias! */
	    }
	    i = k1;
	}
    }
#endif

#ifdef DEBUG_SYM 
        printf("DEBUG (%d): ",action);
        if(key) {
          printf("[key] is [");
          for(loop=0; key[loop] != EOL; loop++) 
             printf("%c",(key[loop] == DELIM) ? '!' : key[loop]);
          printf("]\r\n");
        } else printf("No key passed in.\r\n");
        if(data) {
          printf("[data] (datalen) is [");
          for(loop=0; data[loop] != EOL; loop++) printf("%c",data[loop]);
          printf("] (%d)\r\n",stlen(data));
          printf("[Numeric?] is [%d]\r\n",is_numeric(data));
        } else printf("No data passed in.\r\n");
#endif 

    switch (action) {
    case get_sym:			/* retrieve */
#ifdef EXPERIMENTAL_SETGET
      get = seek_string(SymTable, key, NULL);
      data_copy = data;
      if(!get) { 
#ifdef DEBUG_SYM
        printf("seek_string returned NULL\r\n");
#endif
	ierr = ierr < 0 ? UNDEF - CTRLB : UNDEF;
	data[0] = EOL;
	return;
      } else { 
        if(get->rc && get->rc->reference) { 
          /* Make sure the reference isn't corrupted */
          if(get->rc->reference->datalen != get->datalen) 
            get->datalen = get->rc->reference->datalen;
          if(get->rc->reference->rc->numeric != get->rc->numeric) 
            get->rc->numeric = get->rc->reference->rc->numeric;
          if(get->rc->reference->next_data != get->next_data) 
            get->next_data = get->rc->reference->next_data;
          if(get->rc->reference->rc->status != get->rc->status)
            get->rc->status = get->rc->reference->rc->status;
        }
#ifdef NUMERIC
        if(get->rc && (get->rc->status & STAT_NUMERIC)) {
          sprintf(data, "%d\201", get->rc->numeric);
        } else 
#endif
        if(!get->data) data_copy[0] = EOL; 
        else while(get) { 
          if(get->next_data) {
            memcpy(data_copy, get->data, ST_BLOCK);
            data_copy += ST_BLOCK;
            get=get->next_data;
          } else { 
            memcpy(data_copy, get->data, (unsigned)get->datalen);
            data_copy += get->datalen; 
            *data_copy = EOL; data_copy++;
            get=NULL;
          }
        }
      }
      return; 
#else
      /* OLD get_sym routine */     
	if ((i = alphptr[(int) key[0]])) {
	    k = 1;
	    j = i + 1;			/* first char always matches! */
	    do {
		while (key[k] == partition[++j]) {	/* compare keys */
		    if (key[k] == EOL) {
			i = UNSIGN (partition[++j]);
			if (i < 4) {
			    k = 0;
			    while (k < i)
				data[k++] = partition[++j];
			} else
			    stcpy0 (data, &partition[j + 1], i);
			data[i] = EOL;
			return;
		    }
		    k++;
		}
		i += UNSIGN (partition[i]);	/* skip key */
		i += UNSIGN (partition[i]) + 1;		/* skip data */
		k = 0;
		j = i;
	    } while (i < PSIZE);
	}
	ierr = ierr < 0 ? UNDEF - CTRLB : UNDEF;
	data[0] = EOL;
	return;
#endif

#ifdef EXPERIMENTAL_SETGET
    old0:  /* DUMMY GoTo points!  This breaks things! */
    k_entry: printf("used GOTO not available!\r\n"); exit(1);
#endif

    case set_sym:			/* store/create variable; */
	datal = stlen (data);		/* data length */
#if 0 /* this works */
        for(i0=0; key[i0] != EOL && key[i0] != DELIM; i0++) tmp1[i0] = key[i0];
        tmp1[i0] = EOL;
        printf("[tmp1] is [");
        for(loop=0; tmp1[loop] != EOL; loop++) printf("%c",tmp1[loop]);
        printf("]\r\n");
#endif
#ifdef EXPERIMENTAL_SETGET
        if(!SymTable) { 
           SymTable = NEW(subscript,1);
#ifdef DEBUG_SYM
           printf("Created new SymTable\r\n");
#endif
        }
        new_string = find_home(SymTable, key);
#ifdef NUMERIC
        if(is_numeric(data)) {
          long_tmp = str2long(data);
          new_string->rc->numeric = long_tmp;
          if(new_string->rc && new_string->rc->reference) 
            new_string->rc->reference->rc->numeric = long_tmp;
          if(new_string->data) free(new_string->data);
          if(!new_string->rc->status & STAT_NUMERIC) 
            new_string->rc->status += STAT_NUMERIC;
          new_string->data = NULL;
        } else {
#endif
          new_string->rc->numeric = 0;
          if(new_string->rc->status & STAT_NUMERIC) 
            new_string->rc->status -= STAT_NUMERIC;
          if(new_string->rc && new_string->rc->reference) 
            new_string->rc->reference->rc->numeric = 0;
          data_copy = data;
          while(datal > ST_BLOCK) { 
            if(!new_string->data) 
              new_string->data = NEW(char, ST_BLOCK);
            if(!new_string->next_data)
              new_string->next_data = NEW(string,1);
            if(new_string->rc && new_string->rc->reference) 
              new_string->rc->reference->next_data = new_string->next_data;
            memcpy(new_string->data, data_copy, ST_BLOCK);
            new_string = new_string->next_data;
            datal -= ST_BLOCK; data_copy += ST_BLOCK;
          }
#ifdef DEBUG_SYM
          printf("[datal] datal is [%d]\r\n",datal);
#endif
          new_string->datalen = datal;
          if(new_string->rc && new_string->rc->reference) 
            new_string->rc->reference->datalen = datal;
          if(!new_string->data) {
#ifdef DEBUG_SYM
            printf("Alloc'd new memory for remaining string bit\r\n");
            printf("Datalen is (%d)\r\n",new_string->datalen);
#endif
            new_string->data = NEW(char,ST_BLOCK);
          }
          memcpy(new_string->data, data_copy, datal+1);
#ifdef DEBUG_SYM
        printf("[data] of last string seg is [");
        for(start = new_string->data; *start != EOL; start++)
          printf("%c",*start);
        printf("]\r\n");
        if(new_string->rc && new_string->rc->next_node) { 
          printf("The next node is [");
          for(start = new_string->rc->next_node->rc->key; *start != EOL; start++)
            printf("%c",*start);
          printf("]\r\n");
        } else printf("There is no next node.\r\n"); 
#endif
#ifdef NUMERIC
      }
#endif
        if(new_string->next_data) {  /* Extra memory can be liberated */
          tmp = new_string;
          new_string = new_string->next_data;
          tmp->next_data = NULL; /* Cut off the string */
          while(new_string != NULL) { /* Free the remaining data */
            free_string = new_string;
            new_string = new_string->next_data;
            free(free_string);
          }
        }
        return;
#else /* Experimental SetGet */
       
        /* Old set_sym routine */
/* check whether the key has subscripts or not */
	if ((keyl = stlen (key) + 2) > STRLEN) { ierr = MXSTR; return; }
	k1 = 0;
	i = 1;
	while (key[i] != EOL) {
	    if (key[i++] == DELIM) {
		k1 = i;
		break;
	    }
	}

	if ((i = alphptr[(int) key[0]])) {	/* previous entry */
	    j = i + 1;
	    k = 1;
	} else {
	    i = symlen;
	    j = i;
	    k = 0;
	}

	if (k1 == 0)			/* key was unsubscripted */
	    while (i < PSIZE)
/* compare keys */
	    {
		while (key[k] == partition[++j]) {
		    if (key[k] == EOL) goto old;
		    k++;
		}
		if (key[k] < partition[j]) break;
		i += UNSIGN (partition[i]);	/* skip key */
		i += UNSIGN (partition[i]) + 1;		/* skip data */
		j = i;
		k = 0;
	} else				/* key was subscripted */
	    while (i < PSIZE)
/* compare keys */
	    {
		while (key[k] == partition[++j]) {
		    if (key[k] == EOL)
			goto old;
		    k++;
		}
		if (k < k1) {
		    if (key[k] < partition[j])
			break;
		} else {
		    long    m,
		            n,
		            o,
		            ch;

/* get complete subscripts */
		    n = k;
		    while (key[--n] != DELIM) ;
		    n++;
		    m = j + n - k;
		    o = 0;
		    while ((ch = tmp3[o++] = partition[m++]) != EOL && ch != DELIM) ;
		    if (ch == DELIM)
			tmp3[--o] = EOL;
		    o = 0;
		    while ((ch = tmp2[o++] = key[n++]) != EOL && ch != DELIM) ;
		    if (ch == DELIM)
			tmp2[--o] = EOL;
		    if (collate (tmp3, tmp2) == FALSE) {
			if (stcmp (tmp2, tmp3) || ch == EOL)
			    break;
		    }
		}
		i += UNSIGN (partition[i]);	/* skip key */
		i += UNSIGN (partition[i]) + 1;		/* skip data */
		j = i;
		k = 0;
	    }
/* if    entry found,     i pointer to searched entry
 * else  entry not found, i pointer to alphabetically next entry */
/* new entry */
	if (setop) {
	    tmp1[0] = EOL;
	    m_op (tmp1, data, setop);
	    setop = 0;
	    if (ierr > OK)
		return;
	    datal = stcpy (data, tmp1);
	}
	k = i;
	j = key[0];
	i = keyl + datal + 1;
	if (alphptr['%'])
	    alphptr['%'] -= i;
	for (k1 = 'A'; k1 <= j; k1++) {
	    if (alphptr[k1])
		alphptr[k1] -= i;
	}
	i = k - i;
	if (alphptr[j] == 0 || alphptr[j] > i)
	    alphptr[j] = i;

	j = keyl + datal + 1;
	i = symlen - j;
	if (i <= 256) {			/* more space needed. try to get it */
	    long    dif = 0L;

	    dif = getpmore ();
	    if (dif == 0) {
		ierr = STORE;
		return;
	    }
	    i += dif;
	    k += dif;
	}
	symlen = i;
	s = &partition[i] - 256;
	stcpy0 (&partition[i], &partition[j + i], k - i);
	i = k - (keyl + datal + 1);
	partition[i++] = (char) (keyl);
	stcpy (&partition[i], key);	/* store new key */
	i += keyl - 1;

	partition[i++] = (char) (datal);
	stcpy0 (&partition[i], data, datal);	/* store new data */
	return;

/* there is a previous value */
      old:i += UNSIGN (partition[i]);
	if (setop) {
	    j = UNSIGN (partition[i]);
	    stcpy0 (tmp1, &partition[i + 1], j);
	    tmp1[j] = EOL;
	    m_op (tmp1, data, setop);
	    setop = 0;
	    if (ierr > OK)
		return;
	    datal = stcpy (data, tmp1);
	}
      old0:				/* entry from getinc */
	j = UNSIGN (partition[i]) - datal;
	if (j < 0) {			/* more space needed */
	    if ((symlen + j) <= 256) {
		long    dif = 0L;

		dif = getpmore ();
		if (dif == 0L) {
		    ierr = STORE;
		    return;
		}
		i += dif;
	    }
	    for (k = 36; k < key[0]; k++) {
		if (alphptr[k])
		    alphptr[k] += j;
	    }
	    if (alphptr[k] && alphptr[k] < i)
		alphptr[k] += j;

	    stcpy0 (&partition[symlen + j], &partition[symlen], i - symlen);
	    i += j;
	    symlen += j;
	    s = &partition[symlen] - 256;
	    tryfast = 0;
	} else if (j > 0) {		/* surplus space */
/* in a dynamic environment it is sufficient to          */
/* set newdatalength=olddatalength                       */ 
            if (nocompact) {
		datal += j;
	    }
/* instead of compression of the local symbol table,     */
/* which the following piece of code does                */
	    else {
		symlen += j;
		s = &partition[symlen] - 256;
		for (k = 36; k < key[0]; k++) {
		    if (alphptr[k])
			alphptr[k] += j;
		}
		if (alphptr[k] && alphptr[k] < i)
		    alphptr[k] += j;
		i += j;
		k = i;
		j = i - j;
		while (i >= symlen) {
		    partition[i--] = partition[j--];
		}
		i = k;
		tryfast = 0;
		nocompact = TRUE;
	    }
	}
	partition[i++] = (char) (datal);
	j = datal;
	if (j < 4) {
	    k = 0;
	    while (k < j)
		partition[i++] = data[k++];
	    return;
	}
	stcpy0 (&partition[i], data, j);	/* store new data */
	return;
#endif
/* end of set_sym section */
    case dat:
#ifdef NEWSTACK
        get = seek_string(SymTable, key, NULL);
        if(!get) { /* Symbol doesn't exist */
          data[0] = '0'; data[1] = EOL;
        } else if(!get->rc->subs) { /* Symbol exists, has no subscripts */
          if(DATA(get)) { data[0] = '1'; data[1] = EOL; }
          else { data[0] = '0'; data[1] = EOL; }
        } else { /* Symbol exists, has subscripts */
          data[0] = '1';
          if(DATA(get)) { data[1] = '1'; data[2] = EOL; }
          else { data[1] = '0'; data[1] = EOL; }
        }
        return;
#else
/* note: we assume EOL<DELIM<ASCII */
	data[0] = '0';
	data[1] = EOL;
	if ((i = alphptr[(int) key[0]])) {
	    data[2] = EOL;
	    j = i + 1;
	    k = 1;
	    do {
		while ((k1 = key[k] - partition[++j]) == 0) {	/* compare keys */
		    if (key[k] == EOL)
			break;
		    k++;
		}
		if (k1 == 0)
		    data[0] = '1';
		else {
		    if (partition[j] == DELIM && key[k] == EOL) {
			data[1] = data[0];
			data[0] = '1';
			return;
		    }
		    if (k1 < 0 && k < 2)
			return;
		}
		i += UNSIGN (partition[i]);	/* skip key */
		i += UNSIGN (partition[i]) + 1;		/* skip data */
		j = i;
		k = 0;
	    }
	    while (i < PSIZE);
	}
	return;
#endif
/* end of $data section */

    case merge_sym:			/* merge */
#ifdef DEBUG_SYM
        printf("DEBUG MERGE: ");
        printf("[key] is [");
        for(loop=0;key[loop]!=EOL;loop++) 
          printf("%c",(key[loop] == DELIM) ? '!' : key[loop]);
        printf("]\r\n");
        printf("[data] is [");
        for(loop=0;data[loop]!=EOL;loop++) 
          printf("%c",(data[loop] == DELIM) ? '!' : data[loop]);
        printf("]\r\n");
#endif

        get = seek_string(SymTable, data, &ref_pointer);
        if(!get) return;
        new_string = find_home(SymTable, key);
        if(!new_string) return;
        copy_string(new_string, get);
#ifdef DEBUG_SYM
        printf("MERGE OK\r\n");
#endif
        return;

    case getinc:			/* increment by one and retrieve */
#ifdef DEBUG_SYM
        printf("DEBUG GETINC: ");
        printf("[key] is [");
        for(loop=0;key[loop]!=EOL;loop++) 
          printf("%c",(key[loop] == DELIM) ? '!' : key[loop]);
        printf("]\r\n");
#endif
#ifdef EXPERIMENTAL_SETGET
        get = seek_string(SymTable, key, NULL);
        if(!get) get = find_home(SymTable, key);  
        if(!get) return; /* Out of memory */
        if(!get->rc) get->rc = NEW(string_rc,1);
        if(!get->rc->numeric) { 
          /* Trash an existing string value */
          if(get->data) { free(get->data); get->data = NULL; }
          tmp = get->next_data;
          while(tmp) {
            free_string = tmp; tmp = tmp->next_data;
            free(free_string);
          }
        }
        get->rc->numeric++;
        if(!get->rc->status & STAT_NUMERIC) get->rc->status += STAT_NUMERIC;
        sprintf(data, "%d\201", get->rc->numeric);
        return;    
#else
	if ((i = alphptr[(int) key[0]])) {
	    j = i + 1;
	    k = 1;
	    do {
		while (key[k] == partition[++j]) {	/* compare keys */
		    if (key[k] == EOL) {
			i = UNSIGN (partition[++j]);
			stcpy0 (data, &partition[j + 1], i);
			data[i] = EOL;	/* data retrieved ... now increment */
/****************increment by one*******************/
			if (i == 0)
			    i++;	/* if data was empty  use EOL as dummy value */
			if (i > 1 && data[0] == '0')
			    i++;	/* leading zero  use EOL as dummy value */
			k = 0;
			while (k < i) {
			    if ((k1 = data[k++]) < '0' || k1 > '9') {	/* no positive integer */
				numlit (data);
				tmp1[0] = '1';
				tmp1[1] = EOL;
				add (data, tmp1);
				datal = stlen (data);
				i = j;
				nocompact = FALSE;	/* getinc needs compacted symtab */
				goto old0; 
			    }
			}
			k1 = k--;	/* length of string */
			while ((partition[j + 1 + k] = ++data[k]) > '9') {
			    partition[j + 1 + k] = '0';
			    data[k--] = '0';
			    if (k < 0) {
				k = k1;
				while (k >= 0) {
				    data[k + 1] = data[k];
				    k--;
				}
				data[0] = '1';
				s = &partition[--symlen] - 256;
				if (alphptr['%'])
				    alphptr['%']--;
				for (k = 'A'; k <= key[0]; k++) {
				    if (alphptr[k])
					alphptr[k]--;
				}
				k = j - 1;
				j = symlen;
				stcpy0 (&partition[j], &partition[j + 1], k - j);
				partition[k] = (char) ++i;
				partition[++k] = '1';
				return;
			    }
			}
			return;
/************end increment by one*******************/
		    }
		    k++;
		}
/** if (key[k]<partition[j]) break; **/
		i += UNSIGN (partition[i]);	/* skip key */
		i += UNSIGN (partition[i]) + 1;		/* skip data */
		j = i;
		k = 0;
	    } while (i < PSIZE);
	}
	data[0] = EOL;
	ierr = UNDEF;
	return;
#endif

    case order:			/* next one please */
#ifdef NEWSTACK
        data_copy = NEW(char,stlen(key)+1);
        stcpy(data_copy, key);
        flag = 0;
        for(loop=0; data_copy[loop] != EOL; loop++)
          if(data_copy[loop] == DELIM && data_copy[loop+1] == EOL) {
            flag++; data_copy[loop] = EOL;
          }
        loc_head = seek_leaf(SymTable, data_copy, NULL);
        if(!loc_head) { 
          get = NULL;
        } else {
          for(loc_tmp = loc_head; loc_tmp; loc_tmp = loc_tmp->child) {
             loc_last = loc_tmp;
          }
          if(flag) {
            get = string_end(loc_last->branch->rc->subs,(ordercnt == 1));
          } else {
            if(ordercnt > 0) get = get_next(loc_last->branch, loc_last->parent);
            else get = get_previous(loc_last->branch, loc_last->parent);
          } 
        }
        /* Free the locator memory */
        loc_tmp = loc_head;
        while(loc_tmp) {
          loc_last = loc_tmp; loc_tmp = loc_tmp->child; free(loc_last);
        }
        if(!get) data[0] = EOL; 
        else stcpy(data, get->rc->key); 
        return;
#else
	if (ordercnt < 0)
	    goto zinv;
	k1 = (j = stcpy (tmp1, key) - 1);
	while (tmp1[k1] != DELIM)
	    if ((--k1) <= 0)
		goto unsubscr;
	tmp1[++k1] = EOL;
	stcpy (tmp2, &key[k1]);
	if (ordercnt == 0) {
	    stcpy (data, tmp2);
	    l_o_val[0] = EOL;
	    return;
	}
	data[0] = EOL;
	if ((i = alphptr[(int) key[0]]) == 0) {
	    l_o_val[0] = EOL;
	    return;
	}
/***************************/
/* frequent special case: the key of which we search the next
 * entry is defined ! */
	if (tmp2[0] != EOL) {
	    if (tryfast && stcmp (key, &partition[tryfast + 1]) == 0) {
		j = tryfast;
		j += UNSIGN (partition[j]);	/* skip key */
		j += UNSIGN (partition[j]) + 1;		/* skip data */
		goto begorder;
	    }
	    k = 1;
	    j = i + 1;			/* first char always matches! */
	    do {
		while (key[k] == partition[++j]) {	/* compare keys */
		    if (key[k++] == EOL) {
			j = i;
			goto begorder;
		    }
		}
		i += UNSIGN (partition[i]);	/* skip key */
		i += UNSIGN (partition[i]) + 1;		/* skip data */
		k = 0;
		j = i;
	    } while (i < PSIZE);
	}
/* the key was not defined */
/***************************/
	j = alphptr[(int) key[0]];
      begorder:;
	do {
	    if (key[0] != partition[j + 1]) {
		l_o_val[0] = EOL;
		data[0] = EOL;
		return;
	    }
	    stcpy0 (data, &partition[j + 1], k1);
	    data[k1] = EOL;

	    if (stcmp (tmp1, data) == 0) {
		stcpy (data, &partition[j + 1 + k1]);	/* index on same level */
		k = 0;
		while (data[k] != EOL && data[k] != DELIM)
		    k++;
		data[k] = EOL;
		if (collate (tmp2, data)) {
		    if (--ordercnt <= 0) {
			tryfast = j;
/* save data value for inspection with $V(110) */
			j += UNSIGN (partition[j]);	/* skip key */
			k = UNSIGN (partition[j++]);
			stcpy0 (l_o_val, &partition[j], k);
			l_o_val[k] = EOL;
			return;
		    }
		    ordercounter++;
		}
	    }
	    j += UNSIGN (partition[j]);	/* skip key */
	    j += UNSIGN (partition[j]) + 1;	/* skip data */
	}
	while (j < PSIZE);
	data[0] = EOL;
	tryfast = 0;
	l_o_val[0] = EOL;
	return;
#endif
/* end of $order section */

    case kill_all:
      genocid:
#ifdef EXPERIMENTAL_SETGET
        if(SymTable) { 
           kill_subscript(SymTable); free(SymTable); SymTable = NULL; 
        }
        return;
#else
        /* Old genocide routine */
	alphptr['%'] = 0;
	for (i = 'A'; i <= 'z'; alphptr[i++] = 0) ;
	symlen = PSIZE;
	s = &partition[symlen] - 256;
	tryfast = 0;
	return;
#endif

    case kill_sym:			/* kill them dirty bloody variables */
#ifdef EXPERIMENTAL_SETGET
        tmp = seek_string(SymTable, key, &ref_pointer);
        if(!tmp) return; /* String not found */ 
        *ref_pointer = tmp->rc->next_node; /* close the gap in linked list */
        kill_string(tmp);
        return;
#else
        /* Old Kill Routine */ 

	if ((i = alphptr[(int) key[0]]) == 0)
	    return;			/* damn - nothing to kill */
	kill_from = 0;
	while (i < PSIZE) {
	    j = i;
	    k = 0;
	    while ((k1 = key[k]) == partition[++j]) {	/* compare keys */
		if (k1 == EOL)
		    break;
		k++;
	    }
	    if (k1 == EOL && (partition[j] == DELIM || partition[j] == EOL)) {
		if (kill_from == 0)
		    kill_from = i;
	    } else {
		if (kill_from)
		    break;
	    }
	    i += UNSIGN (partition[i]);	/* skip key */
	    i += UNSIGN (partition[i]) + 1;	/* skip data */
	}
      k_entry:;			/* entry from killone section */
	if (kill_from) {
	    j = i - kill_from;
	    symlen += j;
	    s = &partition[symlen] - 256;
	    for (k = 36; k < key[0]; k++) {
		if (alphptr[k])
		    alphptr[k] += j;
	    }
	    if (alphptr[k] == kill_from) {
		alphptr[k] = i;
		if (partition[i + 1] != key[0])
		    alphptr[k] = 0;
	    } else {
		alphptr[k] += j;
	    }
/*         j=i-j; while(i>symlen) partition[--i]=partition[--j];  */
	    stcpy1 (&partition[i - 1], &partition[i - j - 1], i - symlen);
	}
	tryfast = 0;
	return;
#endif
/* end of kill_sym section */

    case killone:			/* kill one variable, not descendants */
#ifdef EXPERIMENTAL_SETGET
        tmp = seek_string(SymTable, key, &ref_pointer);
        if(!tmp) return; /* String not found */ 
	free(tmp->data); tmp->data = NULL; tmp->datalen = 0;
        get = tmp; tmp = tmp->next_data;
        while(tmp) { 
          free_string = tmp; free(tmp->data);
          tmp=tmp->next_data; free(free_string); 
        }
        tmp = get; 
        if(!tmp->rc || !tmp->rc->subs) { /* Kill variable if no descendants */
          *ref_pointer = tmp->rc->next_node; /* close the gap in linked list */
          if(tmp->rc) {
            if(tmp->rc->key) free(tmp->rc->key);
            free(tmp->rc);
          }
          free(tmp);
        }
        return;
#else
	if ((i = alphptr[(int) key[0]]) == 0)
	    return;			/* nothing to kill */
	kill_from = 0;
	while (i < PSIZE) {
	    j = i;
	    k = 0;
	    while ((k1 = key[k]) == partition[++j]) {	/* compare keys */
		if (k1 == EOL)
		    break;
		k++;
	    }
	    k = i;
	    i += UNSIGN (partition[i]);	/* skip key */
	    i += UNSIGN (partition[i]) + 1;	/* skip data */
	    if (k1 == EOL) {
		if (partition[j] == DELIM)
		    return;		/* descendant */
		kill_from = k;
		goto k_entry; 
	    }
	}
	tryfast = 0;
	return;
#endif /* NEWSTACK */
/* end of killone section */

    case killexcl:			/* exclusive kill */
#ifdef NEWSTACK
#ifdef DEBUG_SYM
        printf("DEBUG killexcl: ");
        printf("key is [");
        for(loop1=0; key[loop1] != EOL; loop1++) 
         printf("(%c-%d)",(key[loop1] != DELIM) ? key[loop1] : '!', key[loop1]);
        printf("]\r\n");
#endif /* DEBUG */
        loop1=0; last_stack_node = NULL; stack_node = NULL;
        while(key[loop1] != EOL) { 
          while(key[loop1] == ' ') loop1++;
          loop2=0;
          while((key[loop1] != ' ') && (key[loop1] != EOL)) {
            tmp1[loop2] = key[loop1];
            loop1++; loop2++;
          }
          if(loop2) { 
            tmp1[loop2] = EOL; tmp1[loop2+1] = '\0'; 
            if((tmp = seek_string(SymTable, tmp1, NULL))) {
#ifdef DEBUG_SYM
              printf("Initting: Found [%s]\r\n",tmp1); 
#endif /* DEBUG */
              stack_node = NEW(st_stack_node,1);
              stack_node->next = last_stack_node; last_stack_node = stack_node;
              stack_node->variable = tmp;
              stack_node->full_key = NEW(char,stlen(tmp)+1);
              stcpy(stack_node->full_key, tmp1);
              /* stack_node->full_key = tmp1; */
#ifdef DEBUG_SYM
              printf("Pushed variable onto stack.\r\n");
#endif /* DEBUG */
            } 
#ifdef DEBUG_SYM
            else printf("Initting: DIDN'T find [%s]\r\n",tmp1);
#endif /* DEBUG */
          }
        }
        symtab(kill_all, NULL, NULL); /* Kill everything */
	SymTable = NEW(subscript, 1);
        stack_node = last_stack_node; 
        while(stack_node) {
          new_string = find_home(SymTable, stack_node->full_key);  
          if(!new_string) { printf("PANIC::new_string nonexistant!"); } 
          new_string->datalen = stack_node->variable->datalen;
          new_string->data = stack_node->variable->data;
          new_string->next_data = stack_node->variable->next_data;
          new_string->rc->subs = stack_node->variable->rc->subs;
          last_stack_node = stack_node; stack_node = stack_node->next;
          free(last_stack_node);
        }
        return;
#else
	i = symlen;
	while (i < PSIZE) {
	    tmp2[0] = SP;
	    kill_from = i;
	    stcpy (tmp3, &partition[i + 1]);
	    stcpy (&tmp2[1], tmp3);
	    stcat (tmp2, " \201");
	    i += UNSIGN (partition[i]);
	    i += UNSIGN (partition[i]) + 1;
	    if (kill_ok (key, tmp2) == 0)
		continue;		/* don't kill */

	    while (i < PSIZE) {
		j = i;
		k = 0;
		while ((k1 = tmp3[k]) == partition[++j]) {	/* compare keys */
		    if (k1 == EOL)
			break;
		    k++;
		}
		if (k1 != EOL || (partition[j] != DELIM && partition[j] != EOL))
		    break;
		i += UNSIGN (partition[i]);	/* skip key */
		i += UNSIGN (partition[i]) + 1;		/* skip data */
	    }
	    j = i - kill_from;
	    symlen += j;
	    s = &partition[symlen] - 256;
	    for (k = 36; k < tmp3[0]; k++) {
		if (alphptr[k])
		    alphptr[k] += j;
	    }
	    if (alphptr[k] == kill_from) {
		alphptr[k] = i;
		if (partition[i + 1] != tmp3[0])
		    alphptr[k] = 0;
	    } else {
		alphptr[k] += j;
	    }
	    stcpy1 (&partition[i - 1], &partition[i - j - 1], i - symlen);
	    i = kill_from + j;
	}
	tryfast = 0;
	return;
/* end of killexcl section */
#endif /* NEWSTACK */

    case query:			/* next entry */
    case bigquery:
#ifdef NEWSTACK
        data_copy = NEW(char,stlen(key)+1);
        stcpy(data_copy, key);
        flag = 0;
        for(loop=0; data_copy[loop] != EOL; loop++)
          if(data_copy[loop] == DELIM && data_copy[loop+1] == EOL) {
            flag++; data_copy[loop] = EOL;
          }
        loc_head = seek_leaf(SymTable, data_copy, NULL);
        if(!loc_head) get = NULL;
        else {
          loc_last = NULL;
          for(loc_tmp = loc_head; loc_tmp; loc_tmp = loc_tmp->child)
             loc_last = loc_tmp;
          if(flag) {
            get = string_end(loc_last->branch->rc->subs,(ordercnt == 1));
            /* if !get, we have no descendants */
          } else {
            if(ordercnt > 0) { 
              /* We want the next in sequence */
              /* Free up everything from here on */
              loc_tmp = loc_last->child;
              while(loc_tmp) { 
                loc_free = loc_tmp; loc_tmp = loc_tmp->child; free(loc_free);
              }
              /* First descend subscripts, Our first subscript counts */
              if(loc_last->branch->rc->subs) {
               loc_tmp=get_first_string(loc_last->branch->rc->subs, &get, NULL);
               if(get && loc_tmp) {
                 loc_last->child = loc_tmp;
                 loc_tmp->parent = loc_last;
                 while(loc_last->child) loc_last = loc_last->child;
               }
              } else get = NULL; 
              /* If that fails, look for the next entry */
              while(!get && loc_last->parent) {
                get = get_next(loc_last->branch, loc_last->parent);
                while(get && !DATA(get)) {
                  get = get_next(get, loc_last->parent);
                  if(get && !DATA(get)) {
                    loc_tmp = get_first_string(get->rc->subs, &get, NULL);
                    if(get && DATA(get)) {
                      loc_last->child = loc_tmp;
                      loc_tmp->parent = loc_last;
                      while(loc_last->child) loc_last = loc_last->child;
                    }
                  }
                }
                loc_tmp = loc_last; loc_last = loc_last->parent;
                loc_last->child = NULL; free(loc_tmp);
              }
            } else {
              get = NULL;
              while(!get && loc_last->parent) {
                /* Get previous entry on this level */
                get = get_previous(loc_last->branch, loc_last->parent);
                while(get && DATA(get)) {
                  /* If it has subscripts, they come before our target */
                  loc_tmp = get_last_string(get->rc->subs, &tmp, NULL);
                  if(tmp && DATA(tmp)) { 
                    get = tmp; loc_last->child = loc_tmp;
                    loc_tmp->parent = loc_last;
                    while(loc_last->child) loc_last = loc_last->child;
                  }
                  if(!DATA(get)) get=get_previous(get, loc_last->parent);
                }
                loc_tmp = loc_last; loc_last = loc_last->parent;
                loc_last->child = NULL; free(loc_tmp);
                if(DATA(loc_last->branch)) { 
                  get = loc_last->branch; 
                  loc_tmp = loc_last; loc_last = loc_last->parent;
                  if(loc_last) loc_last->child = NULL; 
                  else loc_head = NULL;
                  free(loc_tmp);
                }
              }
            }
          } 
        }
        if(!get) data[0] = EOL; 
        else { 
          data_copy = data;
          loc_tmp = loc_head;
          if(!loc_tmp) {
            data_copy += stcpy(data_copy, get->rc->key);
          } else {
            data_copy += stcpy(data_copy, loc_tmp->branch->rc->key);
            loc_tmp = loc_tmp->child;
            *data_copy = '('; data_copy++;
            while(loc_tmp && (loc_tmp != loc_last)) {
              data_copy += stcpy(data_copy, loc_tmp->branch->rc->key);
              *data_copy = ','; data_copy++;
              loc_tmp = loc_tmp->child;
            }
            if(loc_tmp) { 
              data_copy += stcpy(data_copy, loc_tmp->branch->rc->key);
              *data_copy = ','; data_copy++;
            }
            data_copy += stcpy(data_copy, get->rc->key); 
            *data_copy = ')'; data_copy[1] = EOL;
          } 
        }
        /* Free the locator memory */
        loc_tmp = loc_head;
        while(loc_tmp) {
          loc_last = loc_tmp; loc_tmp = loc_tmp->child; 
          free(loc_last);
        }
        return;
#else
        if (ordercnt == 0) {
            l_o_val[0] = EOL;
            zname(data,key);
            return;
            }
/***************************/
/* frequent special case: the key which we search for is the next
 * entry */

	if ((i = alphptr[(int) key[0]])) {
	    if (stcmp (key, &partition[tryfast + 1]) == 0)
		i = tryfast;
	    else {
		j = i;
		do {
		    if (stcmp (key, &partition[j + 1]) == 0) {
			i = j;
			break;
		    }
		    j += UNSIGN (partition[j]);		/* skip key */
		    j += UNSIGN (partition[j]) + 1;	/* skip data */
		} while (j < PSIZE);
	    }
	} else
	    i = symlen;			/* no previous entry */
/***************************/


/* check whether the key has subscripts or not */
	k1 = 0;
	k = 1;
	while (key[k] != EOL) {
	    if (key[k++] == DELIM) {
		k1 = k;
		break;
	    }
	}

	while (i < PSIZE) {
	    j = i;
	    k = 0;
	    while (key[k] == partition[++j]) {	/* compare keys */
		if (key[k] == EOL)
		    break;
		k++;
	    }
            if (key[k] == EOL) {
                if (partition[j]==EOL) {
		i += UNSIGN (partition[i]);
		i += UNSIGN (partition[i]) + 1;
		}
		break;
	    }
	    if (k < k1 || k1 == 0) {
		if (key[k] < partition[j])
		    break;
	    } else {
		long    m,
		        n,
		        o,
		        ch;

/* get complete subscripts */
		n = k;
		while (key[--n] != DELIM) ;
		n++;
		m = j + n - k;
		o = 0;
		while ((ch = tmp2[o++] = key[n++]) != EOL && ch != DELIM) ;
		if (ch == DELIM)
		    tmp2[--o] = EOL;
		o = 0;
		while ((ch = tmp3[o++] = partition[m++]) != EOL && ch != DELIM) ;
		if (ch == DELIM)
	 	    tmp3[--o] = EOL;
		if (collate (tmp2, tmp3))
		    break;
	    }
	    i += UNSIGN (partition[i]);	/* skip key */
	    i += UNSIGN (partition[i]) + 1;	/* skip data */
	}

/* multiple backward query */
	if (ordercnt < 0) {
	    j = symlen;
	    k = ordercnt - 1;
	    while (j < i) {		/* count entries */
		j += UNSIGN (partition[j]);	/* skip key */
		j += UNSIGN (partition[j]) + 1;		/* skip data */
		k++;
	    }
	    if (k < 0) {
		data[0] = EOL;
		l_o_val[0] = EOL;
		return;
	    }
	    i = symlen;
	    while (--k >= 0) {
		i += UNSIGN (partition[i]);	/* skip key */
		i += UNSIGN (partition[i]) + 1;		/* skip data */
	    }
	}
/* end: multiple backward query */

	while (--ordercnt > 0) {	/* multiple forward $query */
	    if (i >= PSIZE)
		break;
	    i += UNSIGN (partition[i]);	/* skip key */
	    i += UNSIGN (partition[i]) + 1;	/* skip data */
	}

/* now 'i' is pointer to 'next' entry */
	tryfast = i;
/* save data value for inspection with $V(110) */
	j = i;
	j += UNSIGN (partition[j]);
	k = UNSIGN (partition[j]);
	stcpy0 (l_o_val, &partition[j + 1], k);
	l_o_val[k] = EOL;

	keyl = i;
	keyl += UNSIGN (partition[i++]) - 2;

        /* action==bigquery may return a result in a different lvn */
        /* which is illegal with $query() */
        if (action==query) {
           k=0; /* is result same lvn? */
           while (partition[i+k]==key[k]) {
              if (key[k]==DELIM) break;
              k++;
           }
           if (partition[i+k]!=DELIM) i=keyl+1; /* discard result! */
        }
        if (i <= keyl)
             zname(data,&partition[i]);
        else data[0]=EOL;
        return;
#endif
/* end of $query section */

      zinv:;				/* previous one please */
	data[0] = EOL;
	l_o_val[0] = EOL;
	k1 = (j = stcpy (tmp1, key) - 1);
	while (tmp1[k1] != DELIM)
	    if ((--k1) <= 0) {
		ierr = NEXTER;
		return;
	    }
	tmp1[++k1] = EOL;
	stcpy (tmp2, &key[k1]);
	if (tmp2[0] == EOL) {
	    tmp2[0] = DEL;
	    tmp2[1] = DEL;
	    tmp2[2] = EOL;
	}
	k = (int) (key[0]);
	if (alphptr[k] == 0)
	    return;
	j = alphptr[k];
	do {
	    if (key[0] != partition[j + 1])
		goto zinvend;
	    stcpy0 (tmp3, &partition[j + 1], k1);
	    tmp3[k1] = EOL;

	    if (stcmp (tmp1, tmp3) == 0) {
		stcpy (tmp3, &partition[j + 1 + k1]);	/* index on same level */
		k = 0;
		while (tmp3[k] != EOL && tmp3[k] != DELIM)
		    k++;
		tmp3[k] = EOL;
		if (collate (tmp3, tmp2) == FALSE)
		    goto zinvend;
		stcpy (data, tmp3);
/* save data value for inspection with $V(110) */
		i = j;
		i += UNSIGN (partition[i]);
		k = UNSIGN (partition[i]);
		stcpy0 (l_o_val, &partition[i + 1], k);
		l_o_val[k] = EOL;
	    }
	    j += UNSIGN (partition[j]);	/* skip key */
	    j += UNSIGN (partition[j]) + 1;	/* skip data */
	}
	while (j < PSIZE);
      zinvend:
	if (data[0] == EOL)
	    return;
	ordercounter++;
	if (++ordercnt >= 0)
	    return;
	stcpy (&key[k1], data);
	goto zinv;

#ifdef NEWSTACK
    case megapop: /* Pop includes a kill */
        if(!sub_stack) { return; /* Lets not, huh? */ }
        if(SymTable) { kill_subscript(SymTable); free(SymTable); }
        SymTable = sub_stack->sub; 
        sym_node = sub_stack; sub_stack = sub_stack->next; free(sym_node);
        return;
#endif
        
#ifdef EXPERIMENTAL_NEW
    case pop_sym:			/* Pop one symbol off stack */
        /* This automatically kills the last variable w/ same name */
        tmp = seek_string(SymTable, key, &ref_pointer);
        if(!tmp) { 
          /* The string wasn't redefined, so create a dummy */
          /* This is a pretty cheap way of doing this, what I
             really SHOULD do would be to find a place to put
             the string, then insert the old string back, but
             that takes more work than just creating a dummy
             and trashing it, since I already have routines
             to find a place & insert a new variable... */
          new_string = find_home(SymTable, key);  /* new entry */
          tmp = seek_string(SymTable, key, &ref_pointer); 
          if(!tmp) { /* Houston, we have a problem... */
            printf("PANIC in symtab::POP_SYM\r\n");
            return;
          }
        }
        /* Find the stack entry */
        stack_node = string_stack; 
        if(!stcmp(stack_node->full_key, key)) { /* First entry on stack */
          /* Insert the new entry back into the symbol table */
          *ref_pointer = stack_node->variable;

          /* Maintain the link through the old entry */
          stack_node->variable->rc->next_node = tmp->rc->next_node;

          string_stack = stack_node->next;    /* Shift the stack */
          free(stack_node); kill_string(tmp); /* Destroy evidence */
        } else { 
          /* Hunt that sucker down */
          while(stack_node && stcmp(stack_node->full_key, key)) {
            last_stack_node = stack_node;
            stack_node = stack_node->next;
          }
          if(!stack_node) { 
            printf("PANIC in symtab.c with pop_sym"); return; 
          }   /* Oh crap. */
          else { 
            *ref_pointer = stack_node->variable;
            stack_node->variable->rc->next_node = tmp->rc->next_node;
            last_stack_node->next = stack_node->next;
            free(stack_node); kill_string(tmp);
          }
        }
        return;
#endif

/* end of $zinverse section */
    case new_sym:			/* new one symbol */
	if (key[0] == '$') {		/* $svn: save current value on new stack */
#ifdef NEWSTACK	
            new_command = NEW(stack_cmd,1);
            if(!new_command) return; /* This could suck... */
            if((key[1] | 0140) == 't') { 
              new_command->data = NEW(char,2);
              new_command->key  = NEW(char,3);
              new_command->data[0] = test; new_command->data[1] = EOL;
              stcpy(new_command->key, "$t\201");
              new_command->command = STACK_SET;
 	      if (mcmnd != ZNEW) test = FALSE;
            } else if ((key[1] | 0140) == 'j') {	/* NEW $JOB */
              new_command->data = NEW(char,3);
              new_command->key  = NEW(char,3);
              new_command->data[0] = pid/256;
              new_command->data[1] = pid%256;
              new_command->data[2] = EOL;
              stcpy(new_command->data,"$j\201"); 
              new_command->command = STACK_SET;
            } else if (((key[1] | 0140) == 'z') &&	/* NEW $ZINRPT */
		((key[2] | 0140) == 'i')) {
              new_command->data = NEW(char,2);
              new_command->key  = NEW(char,4);
              new_command->data[0] = breakon; new_command->data[1] = EOL; 
	      stcpy (new_command->key, "$zi\201");
	      new_command->command = STACK_SET;
	    } else {
              new_command->data = NEW(char,stlen(zref)+1);
              new_command->key  = NEW(char,4);
	      stcpy (new_command->data, zref);	/* NEW $ZREFERENCE */
	      stcpy (new_command->key, "$zr\201");
              new_command->command = STACK_SET;
	      if (mcmnd != ZNEW) zref[0] = EOL;
            }
            if(!stack) { /* This stack level doesn't exist */
              stack = NEW(stack_level,1);
              stack->previous = NULL;
            } 
            tmp_stack = stack; 
            while(tmp_stack && (tmp_stack->command == FOR)) 
              tmp_stack = tmp_stack->previous; /* Don't add to a FOR level */
            new_command->next = tmp_stack->new;
            tmp_stack->new = new_command;
#else /* NEWSTACK */
	    if (newptr > newlimit && getnewmore ())
		return;
	    if ((key[1] | 0140) == 't') {	/* NEW $TEST */
		*newptr++ = test;
		*newptr++ = EOL;
		*newptr++ = 1;
		k1 = stcpy (newptr, "$t\201");
		newptr += k1;
		*newptr++ = EOL;
		*newptr++ = k1;
		*newptr++ = set_sym;
		if (mcmnd != ZNEW)
		    test = FALSE;
		return;
	    }
	    if ((key[1] | 0140) == 'j') {	/* NEW $JOB */
		*newptr++ = pid / 256;
		*newptr++ = pid % 256;
		*newptr++ = EOL;
		*newptr++ = 2;
		k1 = stcpy (newptr, "$j\201");
		newptr += k1;
		*newptr++ = EOL;
		*newptr++ = k1;
		*newptr++ = set_sym;
		return;
	    }
	    if (((key[1] | 0140) == 'z') &&	/* NEW $ZINRPT */
		((key[2] | 0140) == 'i')) {
		*newptr++ = breakon;
		*newptr++ = EOL;
		*newptr++ = 1;
		k1 = stcpy (newptr, "$zi\201");
		newptr += k1;
		*newptr++ = EOL;
		*newptr++ = k1;
		*newptr++ = set_sym;
		return;
	    }
	    j = stcpy (newptr, zref);	/* NEW $ZREFERENCE */
	    newptr += j;
	    *newptr++ = EOL;
	    *newptr++ = j;
	    k1 = stcpy (newptr, "$zr\201");
	    newptr += k1;
	    *newptr++ = EOL;
	    *newptr++ = nakoffs;
	    k1++;
	    *newptr++ = k1;
	    *newptr++ = set_sym;
	    if (mcmnd != ZNEW)
		zref[0] = EOL;
	    return;
#endif
	}
#ifdef EXPERIMENTAL_NEW
        tmp = seek_string(SymTable, key, &ref_pointer);
        if(tmp) { 
            
#ifndef NEWSTACK /* DONT do the newptr/newlimit stuff */
#ifdef DEBUG_SYM
	    start = newptr;
#endif
            if(newptr > newlimit && getnewmore()) return;
            memcpy(newptr, tmp->data, tmp->datalen); /* DANGER! FIX ME! */
            newptr += tmp->datalen; *newptr = EOL; newptr++;
            *newptr = tmp->datalen; /* DANGER WILL ROBINSON, DANGER */
            newptr++; memcpy(newptr, tmp->key, stlen(tmp->key));
            newptr+=stlen(tmp->key); *newptr = EOL; newptr++;
            *newptr = tmp->datalen; newptr++; *newptr = EOL; newptr++;
            *newptr = stlen(tmp->key) + 2; newptr++; 
            *newptr = set_sym; newptr++;
#ifdef DEBUG_SYM
	    printf("SAVING [newptr] newptr became [");
            while(start < newptr) { printf("%c(%d)",
                 (*start==EOL) ? ('!') : *start, *start); start++; }
            printf("{%d}]\r\n", *(newptr-1));
#endif
#endif /* NOT NEWSTACK */
            /* Pull the string/branch out of commission */
            stack_node = NEW(st_stack_node,1);
            stack_node->full_key = NEW(char,stlen(key)+1);
            stack_node->variable = tmp;
            stcpy(stack_node->full_key,key);
            stack_node->next = string_stack;
            string_stack = stack_node;
            tmp->rc->next_node = NULL; /* Just in case */
            *ref_pointer = tmp->rc->next_node; /* close the gap */

            /* Create a stack command to pop it later */
            new_command = NEW(stack_cmd,1);
            if(!new_command) return; /* This could suck... */
            new_command->key = NEW(char,stlen(key)+1);
            stcpy(new_command->key, key);
            new_command->command = STACK_POP;
            if(!stack) { /* This stack level doesn't exist */
              stack = NEW(stack_level,1);
              stack->previous = NULL;
            }
            tmp_stack = stack; 
            while(tmp_stack && (tmp_stack->command == FOR)) 
              tmp_stack = tmp_stack->previous; /* Don't add to a FOR level */
            new_command->next = tmp_stack->new;
            tmp_stack->new = new_command;
        }
#ifndef NEWSTACK
#ifdef DEBUG_SYM
	    start = newptr;
#endif
        if(newptr > newlimit && getnewmore()) return;
        memcpy(newptr, key, stlen(key));
        newptr += stlen(key); *newptr = EOL; newptr++;
        *newptr = stlen(key); newptr++; *newptr = kill_sym; newptr++;
#ifdef DEBUG_SYM
    printf("KILLING [newptr] newptr became [");
    while(start < newptr) { 
       printf("%c(%d)",(*start == EOL) ? ('!') : *start,*start ); start++; 
    }
    printf("{%d}]\r\n",*(newptr-1));
#endif
#endif /* NOT NEWSTACK */
        /* Create a stack command to kill this new variable later */
        new_command = NEW(stack_cmd,1);
        new_command->key = NEW(char,stlen(key)+1);
        stcpy(new_command->key, key);
        new_command->command = STACK_KILL;
#ifdef DEBUG_SYM
        printf("stack is [%d]\r\n",stack);
#endif
        if(!stack) /* This stack level doesn't exist */
          stack = NEW(stack_level,1);
        new_command->next = stack->new; stack->new = new_command;
#ifdef DEBUG_SYM
        printf("Done creating kill stack command...\r\n");
#endif
        return;
#else /* Experimental New */
	if ((i = alphptr[(int) key[0]])) {	/* is there something to be saved?/killed */
/* always FALSE with special variables    */ kill_from = 0;
	    while (i < PSIZE) {
		j = i;
		k = 0;
		while ((k1 = key[k]) == partition[++j]) {	/* compare keys */
		    if (k1 == EOL)
			break;
		    k++;
		}
		if (k1 == EOL && (partition[j] == DELIM || partition[j] == EOL)) {
		    if (kill_from == 0)
			kill_from = i;
		} else {
		    if (kill_from)
			break;
		}
		if (kill_from) {	/* save current values on new stack */
		    j = UNSIGN (partition[i]);
		    k = i + 1;
		    k1 = j;
		    i += j;
		    j = UNSIGN (partition[i]);
		    if (newptr > newlimit && getnewmore ())
			return;
#ifdef DEBUG_SYM
		    start = newptr;
#endif
		    stcpy0 (newptr, &partition[i + 1], j);
		    newptr += j;
		    *newptr++ = EOL;
		    *newptr++ = j;
		    i += (j + 1);
		    stcpy0 (newptr, &partition[k], k1);
		    newptr += k1;
		    *newptr++ = EOL;
		    *newptr++ = k1;
		    *newptr++ = set_sym;
#ifdef DEBUG_SYM
		    printf("SAVING [newptr] newptr became [");
                    while(start < newptr) { printf("%c(%d)",
                      (*start==EOL) ? ('!') : *start, *start); start++; }
                    printf("{%d}]\r\n", *(newptr-1));
#endif
		} else {
		    i += UNSIGN (partition[i]);		/* skip key */
		    i += UNSIGN (partition[i]) + 1;	/* skip data */
		}
	    }
	    if (kill_from && mcmnd != ZNEW) {
		j = i - kill_from;
		symlen += j;
		s = &partition[symlen] - 256;
		for (k = 36; k < key[0]; k++) {
		    if (alphptr[k])
			alphptr[k] += j;
		}
		if (alphptr[k] == kill_from) {
		    alphptr[k] = i;
		    if (partition[i + 1] != key[0])
			alphptr[k] = 0;
		} else {
		    alphptr[k] += j;
		}
		stcpy1 (&partition[i - 1], &partition[i - j - 1], i - symlen);
	    }
	    tryfast = 0;
	}
	if (newptr > newlimit && getnewmore ())
	    return;
#ifdef DEBUG_SYM
        start = newptr;
#endif
	j = stcpy (newptr, key);
	newptr += j;
	*newptr++ = EOL;
	*newptr++ = j;
	*newptr++ = kill_sym;
#ifdef DEBUG_SYM
    printf("KILLING [newptr] newptr became [");
    while(start < newptr) { 
       printf("%c(%d)",(*start == EOL) ? ('!') : *start,*start ); start++; 
    }
    printf("{%d}]\r\n",*(newptr-1));
#endif
	return;
#endif

/* end of new_sym section */
    case new_all:			/* new all symbols */
#ifdef NEWSTACK
        /* Add a command to pop the stack back later */
        new_command = NEW(stack_cmd,1);
        if(!new_command) return; /* This could suck... */
        new_command->key = NULL;
        new_command->command = STACK_POPALL;
        if(!stack) { /* This stack level doesn't exist */
          stack = NEW(stack_level,1);
          stack->previous = NULL;
        }
        tmp_stack = stack; 
        while(tmp_stack && (tmp_stack->command == FOR)) 
          tmp_stack = tmp_stack->previous; /* Don't add to a FOR level */
        new_command->next = tmp_stack->new;
        tmp_stack->new = new_command;

        /* Move the SymbolTable outta here */
        sym_node = NEW(sub_stack_node, 1);
        if(!sym_node) { return; } /* Out of memory */
        /* Link the entire symbol table to the subscript pointer */
        sym_node->sub = SymTable; sym_node->next = sub_stack;
        sub_stack = sym_node; /* Push it on the stack */
        SymTable = NULL; /* Fry the symbol table.  Piece of cake! */
        return;
#else
	i = symlen;
	while (i < PSIZE) {
	    j = UNSIGN (partition[i]);
	    k = i + 1;
	    k1 = j;
	    i += j;
	    j = UNSIGN (partition[i]);
	    if (newptr > newlimit && getnewmore ())
		return;
	    stcpy0 (newptr, &partition[i + 1], j);
	    newptr += j;
	    *newptr++ = EOL;
	    *newptr++ = j;
	    i += (j + 1);
	    stcpy0 (newptr, &partition[k], k1);
	    newptr += k1;
	    *newptr++ = EOL;
	    *newptr++ = k1;
	    *newptr++ = set_sym;
	}
	*newptr++ = kill_all;
	if (mcmnd == ZNEW)
	    return;
	goto genocid;			/* ... and now kill them all */
#endif
/* end of new_all section */
    case newexcl:			/* new all except specified */
#ifdef NEWSTACK
#ifdef DEBUG_SYM
        printf("DEBUG newexcl: ");
        printf("key is [");
        for(loop1=0; key[loop1] != EOL; loop1++) 
         printf("(%c-%d)",(key[loop1] != DELIM) ? key[loop1] : '!', key[loop1]);
        printf("]\r\n");
#endif /* DEBUG */
        loop1=0; last_stack_node = NULL; stack_node = NULL;
        while(key[loop1] != EOL) { 
          while(key[loop1] == ' ') loop1++;
          loop2=0;
          while((key[loop1] != ' ') && (key[loop1] != EOL)) {
            tmp1[loop2] = key[loop1];
            loop1++; loop2++;
          }
          if(loop2) { 
            tmp1[loop2] = EOL; tmp1[loop2+1] = '\0'; 
            if((tmp = seek_string(SymTable, tmp1, NULL))) {
#ifdef DEBUG_SYM
              printf("Initting: Found [%s]\r\n",tmp1); 
#endif /* DEBUG */
              stack_node = NEW(st_stack_node,1);
              stack_node->next = last_stack_node; last_stack_node = stack_node;
              stack_node->variable = tmp;
              stack_node->full_key = NEW(char,stlen(tmp1)+1);
              stcpy(stack_node->full_key, tmp1);
              /* stack_node->full_key = tmp1; */
#ifdef DEBUG_SYM
              printf("Pushed variable onto stack.\r\n");
#endif /* DEBUG */
            } 
#ifdef DEBUG_SYM
            else printf("Initting: DIDN'T find [%s]\r\n",tmp1);
#endif /* DEBUG */
          }
        }
        symtab(new_all, NULL, NULL); /* New everything */
	SymTable = NEW(subscript,1);
        stack_node = last_stack_node; 
        while(stack_node) {
          new_string = find_home(SymTable, stack_node->full_key);  
          if(!new_string) { printf("PANIC::new_string nonexistant!"); } 
          new_string->datalen = stack_node->variable->datalen;
          new_string->data = stack_node->variable->data;
          new_string->next_data = stack_node->variable->next_data;
          new_string->rc->subs = stack_node->variable->rc->subs;
#ifdef NUMERIC
          new_string->rc->numeric = stack_node->variable->rc->numeric;
          new_string->rc->status  = stack_node->variable->rc->status;
#endif
          new_string->rc->reference = stack_node->variable;
#if 0 /* Test method */
          tmp = seek_string(SymTable, key, &ref_pointer); 
          if(!tmp) { /* Houston, we have a problem... */
            printf("PANIC in symtab::NEWEXCL\r\n"); return;
          }
          *ref_pointer = stack_node->variable;
          stack_node->variable->rc->next_node = tmp->rc->next_node;
#endif
          last_stack_node = stack_node; stack_node = stack_node->next;
          free(last_stack_node);
        }
        return;
#else
	i = symlen;
	while (i < PSIZE) {
	    tmp2[0] = SP;
	    kill_from = i;
	    stcpy (tmp3, &partition[i + 1]);
	    stcpy (&tmp2[1], tmp3);
	    stcat (tmp2, " \201");
	    if (kill_ok (key, tmp2) == 0) {	/* don't new */
		i += UNSIGN (partition[i]);
		i += UNSIGN (partition[i]) + 1;
		continue;
	    }
	    j = UNSIGN (partition[i]);
	    k = i + 1;
	    k1 = j;
	    i += j;
	    j = UNSIGN (partition[i]);
	    if (newptr > newlimit && getnewmore ())
		return;
	    stcpy0 (newptr, &partition[i + 1], j);
	    newptr += j;
	    *newptr++ = EOL;
	    *newptr++ = j;
	    i += (j + 1);
	    stcpy0 (newptr, &partition[k], k1);
	    newptr += k1;
	    *newptr++ = EOL;
	    *newptr++ = k1;
	    *newptr++ = set_sym;

	    while (i < PSIZE) {
		j = i;
		k = 0;
		while ((k1 = tmp3[k]) == partition[++j]) {	/* compare keys */
		    if (k1 == EOL)
			break;
		    k++;
		}
		if (k1 != EOL || (partition[j] != DELIM && partition[j] != EOL))
		    break;

		j = UNSIGN (partition[i]);
		k = i + 1;
		k1 = j;
		i += j;
		j = UNSIGN (partition[i]);
		if (newptr > newlimit && getnewmore ())
		    return;
		stcpy0 (newptr, &partition[i + 1], j);
		newptr += j;
		*newptr++ = EOL;
		*newptr++ = j;
		i += (j + 1);
		stcpy0 (newptr, &partition[k], k1);
		newptr += k1;
		*newptr++ = EOL;
		*newptr++ = k1;
		*newptr++ = set_sym;
	    }
	    if (mcmnd == ZNEW)
		continue;
	    j = i - kill_from;
	    symlen += j;
	    s = &partition[symlen] - 256;
	    for (k = 36; k < tmp3[0]; k++) {
		if (alphptr[k])
		    alphptr[k] += j;
	    }
	    if (alphptr[k] == kill_from) {
		alphptr[k] = i;
		if (partition[i + 1] != tmp3[0])
		    alphptr[k] = 0;
	    } else {
		alphptr[k] += j;
	    }
	    stcpy1 (&partition[i - 1], &partition[i - j - 1], i - symlen);
	    i = kill_from + j;
	}
	tryfast = 0;
	if (newptr > newlimit && getnewmore ())
	    return;
	j = stcpy (newptr, key);
	newptr += (j + 1);
	*newptr++ = j;
	*newptr++ = killexcl;
	return;
#endif
/* end of newexcl section */

    case m_alias:			/* define an alias of a variable */
#ifdef NEWSTACK
        /* 
         * This M-Alias command allows creating a reference to a 
         * subscripted array by default.  IE, this allows you
         * to call a function with:  d ^func(.x(1,2))
         *
         */
#ifdef DEBUG_SYM
        printf("DEBUG m_alias: ");
        printf("[key] is [");
        for(loop=0;key[loop]!=EOL;loop++) 
          printf("%c",(key[loop] == DELIM) ? '!' : key[loop]);
        printf("]\r\n");
#endif
        get = seek_string(SymTable, data, NULL);
        if(!get) { 
#ifdef DEBUG_SYM
           printf("failed to find the old variable..\r\n");
#endif
           return; /* Define an alias to a future variable? */
        }
        new_string = find_home(SymTable, key); /* Clobber existing var! */
        if(!new_string) { 
#ifdef DEBUG_SYM
           printf("failed to find a home...\r\n");
#endif
           return; /* Out of memory */
        }
        new_string->datalen       = get->datalen;
        new_string->data          = get->data;
        new_string->next_data     = get->next_data;
        new_string->rc->subs      = get->rc->subs;
#ifdef NUMERIC
        new_string->rc->numeric	  = get->rc->numeric;
        new_string->rc->status    = get->rc->status;
#endif
        new_string->rc->reference = get;
#ifdef DEBUG_SYM
        printf("Reference created OK.\r\n");
#endif
        return;
#else
/* process stuff */
	if (stcmp (key, data) == 0)
	    return;			/* sorry, that's no alias */
	if (data[0] == EOL) {		/* delete an alias from the table */
	    if (aliases) {		/* there are aliases */
		i = 0;
		while (i < aliases) {
		    k = i;
		    k1 = i + UNSIGN (ali[i]) + 1;
		    j = 0;		/* is current reference an alias ??? */
		    while (ali[++i] == key[j]) {
			if (ali[i] == EOL)
			    break;
			j++;
		    }
/* yes, it is, so resolve it now! */
		    if (ali[i] == EOL && key[j] == EOL) {
			if (aliases > k1)
			    stcpy0 (&ali[k], &ali[k1], aliases - k1);
			aliases -= (k1 - k);
			return;
		    }
		    i = k1;
		}
	    }
	    return;
	}
/* new entry to alias table. there is no check agains duplicate entries */
	i = stlen (key);
	j = stlen (data);
	ali[aliases++] = (char) (i + j + 2);	/* byte for fast skipping */
	stcpy (&ali[aliases], key);
	aliases += (i + 1);
	stcpy (&ali[aliases], data);
	aliases += (j + 1);

/* write note to unmake the alias */
	j = stcpy (newptr, key);
	newptr += (j + 1);
	*newptr++ = j;
	*newptr++ = m_alias;

	return;
#endif
    case zdata:			/* nonstandard data function */
#ifdef NEWSTACK
        get = seek_string(SymTable, key, NULL);
        if(!get) { data[0] = EOL; return; }
        counter_head = NEW(counter,1); 
        if(DATA(get)) counter_head->datanodes = 1;
        else counter_head->datanodes = 0;
        if(get->rc->subs) {
          counter_head->next = NEW(counter,1);
          counter_head->next->datanodes = 0;
          counter_head->next->next = NULL;
          count_subnodes(get->rc->subs, counter_head->next);
        }
        data_copy = data;
        while(counter_head) {
          data_copy += sprintf(data_copy, "%d,", counter_head->datanodes);
          counter_free = counter_head; counter_head = counter_head->next;
          free(counter_free);
        }
        data_copy--; *data_copy = EOL;
        return;
#else
	{
	    long    counties[128];
	    int     icnt,
	            icnt0;

	    i = 0;
	    while (i < 128)
		counties[i++] = 0L;	/* init count;  */
/* note: we assume EOL<DELIM<ASCII */
	    icnt = 0;
	    i = 0;
	    while ((j = key[i++]) != EOL)
		if (j == DELIM)
		    icnt++;
	    if ((i = alphptr[(int) key[0]])) {
		data[2] = EOL;
		j = i + 1;
		k = 1;
		do {
		    icnt0 = j + 1;
		    while ((k1 = key[k] - partition[++j]) == 0) {	/* compare keys */
			if (key[k] == EOL)
			    break;
			k++;
		    }
		    if (k1 == 0)
			counties[0] = 1;
		    else {
			if (partition[j] == DELIM && key[k] == EOL) {
			    int     ch;

			    j = icnt0;
			    icnt0 = 0;
			    while ((ch = partition[j++]) != EOL)
				if (ch == DELIM)
				    icnt0++;
			    if (icnt0 <= icnt)
				break;
			    counties[icnt0 - icnt]++;
			}
/*                  if (k1<0 && k<2) break;     */
		    }
		    i += UNSIGN (partition[i]);		/* skip key */
		    i += UNSIGN (partition[i]) + 1;	/* skip data */
		    j = i;
		    k = 0;
		}
		while (i < PSIZE);
	    }
	    i = 128;
	    while (counties[--i] == 0L) ;
	    lintstr (data, counties[0]);
	    j = 1;
	    tmp1[0] = ',';
	    while (j <= i) {
		lintstr (&tmp1[1], counties[j++]);
		stcat (data, tmp1);
	    }

	    return;
	}				/* end of $zdata section */
#endif
    }					/* end of action switch */

/* return next variable or array name - non standard */
  unsubscr:
    if (standard) {
	ierr = NEXTER;
	return;
    }
    j = key[0];
    data[0] = EOL;
    while (alphptr[j] == 0) {
	if (++j >= DEL)
	    return;
    }
    i = alphptr[j];
    while (i < PSIZE) {
	j = i;
	k = 0;
	while ((k1 = key[k] - partition[++j]) == 0) {	/* compare keys */
	    if (key[k] == EOL)
		break;
	    k++;
	}
	if (k1 < 0 && (partition[j] != DELIM || key[k] != EOL)) {
	    j = i;
	    i = 0;
	    while ((data[i] = partition[++j]) != EOL) {
		if (data[i] == DELIM) {
		    data[i] = EOL;
		    break;
		}
		i++;
	    }
	    return;
	}
	i += UNSIGN (partition[i]);	/* skip key */
	i += UNSIGN (partition[i]) + 1;	/* skip data */
    }
    return;

}					/* end of symtab() */
/******************************************************************************/
short int
collate (s, t)
	char   *s,
	       *t;

/* if 't' follows 's' in MUMPS collating sequence a 1 is returned
 * otherwise 0
 */
{
    short   dif;

    if (s[0] == EOL)
	return (t[0] != EOL);		/* the empty one is the leader! */
    if (t[0] == EOL)
	return FALSE;
    if ((dif = stcmp (t, s)) == 0)
	return FALSE;
    if (numeric (s)) {			/* then come numerics */
	if (numeric (t) == FALSE)
	    return TRUE;
	return comp (s, t);
    }
    if (numeric (t))
	return FALSE;
    return dif > 0;
}					/* end of collate() */
/******************************************************************************/
short int
numeric (str)
	char   *str;			/**

                                         *  boolean function that tests
					 *  whether str is a canonical
					 *  numeric
                                         */
{
    register ptr = 0,
            ch;
    register point;

    if (str[0] == '-')
	ptr = 1;
    if (str[ptr] == EOL)
	return FALSE;
    if (str[ptr] == '0')
	return str[1] == EOL;		/* leading zero */
    point = FALSE;
    while ((ch = str[ptr++]) != EOL) {
	if (ch > '9')
	    return FALSE;
	if (ch < '0') {
	    if (ch != '.')
		return FALSE;
	    if (point)
		return FALSE;		/* multiple points */
	    point = TRUE;
	}
    }
    if (point) {
	if ((ch = str[ptr - 2]) == '0')
	    return FALSE;		/* trailing zero */
	if (ch == '.')
	    return FALSE;		/* trailing point */
    }
    return TRUE;
}					/* end of numeric() */
/******************************************************************************/
short int
comp (s, t)
	char   *s,
	       *t;			/* s and t are strings representing */

					/* MUMPS numbers. comp returns t>s  */
{
    register s1 = s[0],
            t1 = t[0],
            point = '.';

    if (s1 != t1) {
	if (s1 == '-')
	    return TRUE;		/* s<0<t */
	if (t1 == '-')
	    return FALSE;		/* t<0<s */
	if (s1 == point && t1 == '0')
	    return FALSE;		/* s>0; t==0 */
	if (t1 == point && s1 == '0')
	    return TRUE;		/* t>0; s==0 */
    }
    if (t1 == '-') {
	char   *a;

	a = &t[1];
	t = &s[1];
	s = a;
    }
    s1 = 0;
    while (s[s1] > point)
	s1++;				/* Note: EOL<'.' */
    t1 = 0;
    while (t[t1] > point)
	t1++;
    if (t1 > s1)
	return TRUE;
    if (t1 < s1)
	return FALSE;
    while (*t == *s) {
	if (*t == EOL)
	    return FALSE;
	t++;
	s++;
    }
    if (*t > *s)
	return TRUE;
    return FALSE;

}					/* end of comp() */
/******************************************************************************/
void
intstr (str, integ)			/* converts integer to string */
	char   *str;
	short   integ;

{
    if (integ < 0) {
	integ = (-integ);
	*str++ = '-';
    }
    if (integ < 10) {
	*str++ = integ + '0';
	*str = EOL;
	return;
    } else if (integ < 100) {
	str += 2;
    } else if (integ < 1000) {
	str += 3;
    } else if (integ < 10000) {
	str += 4;
    } else {
	str += 5;
    }
    *str = EOL;
    do {
	*(--str) = integ % 10 + '0';
    } while (integ /= 10);
    return;
}					/* end of intstr() */
/******************************************************************************/
void
lintstr (str, integ)			/* converts long integer to string */
	char   *str;
	long    integ;

{
    char    result[11];			/* 32 bit = 10 digits+sign */
    register i = 0;

    if (integ < 0) {
	integ = (-integ);
	*str++ = '-';
    }
    do {
	result[i++] = integ % 10 + '0';
    } while (integ /= 10);
    do {
	*str++ = result[--i];
    } while (i > 0);
    *str = EOL;
    return;
}					/* end of lintstr() */
/****************************************************************/

/* user defined special variable table management */

void
udfsvn (action, key, data)		/* symbol table functions */
	short   action;			/* set_sym      get_sym   */

	char   *key,			/* lvn as ASCII-string */
	       *data;

/* The symbol table is placed at the high end of 'svntable'. It begins at
 * 'svnlen' and ends at 'UDFSVSIZ'. The layout is
 * (keylength)(key...)(<EOL>)(datalength)(data...[<EOL>])
 * The keys are sorted in alphabetic sequence.
 * 
 * To have the same fast access regardless of the position in the
 * alphabet for each character a pointer to the first variable beginning
 * with that letter is maintained. (0 indicates there's no such var.)
 */

{
    long    keyl,			/* length of key                  */
            datal;			/* length of data                 */
    register long int i,
            j,
            k,
            k1;
#ifdef EXPERIMENTAL_SETGET
    string *get, *new_string, *tmp, *free_string;
    char *data_copy;
    long long_tmp;
#endif
#ifdef DEBUG_SYM
    char *start;
#endif

    switch (action) {
    case get_sym:			/* retrieve */
#ifdef EXPERIMENTAL_SETGET
      get = seek_string(UDFSV, key, NULL);
      data_copy = data;
      if(!get) { 
#ifdef DEBUG_SYM
        printf("seek_string returned NULL\r\n");
#endif
	ierr = ierr < 0 ? UNDEF - CTRLB : UNDEF;
	data[0] = EOL;
	return;
      } else { 
        if(get->rc && get->rc->reference) { 
          /* This is why we need a better way to do references */
          /* Make sure the reference isn't corrupted */
          if(get->rc->reference->datalen != get->datalen) 
            get->datalen = get->rc->reference->datalen;
          if(get->rc->reference->rc->numeric != get->rc->numeric) 
            get->rc->numeric = get->rc->reference->rc->numeric;
          if(get->rc->reference->next_data != get->next_data) 
            get->next_data = get->rc->reference->next_data;
        }
#ifdef NUMERIC
        if(get->rc && get->rc->numeric) {
          sprintf(data, "%d\201", get->rc->numeric);
        } else 
#endif
        while(get) { 
          if(get->next_data) {
            memcpy(data_copy, get->data, ST_BLOCK);
            data_copy += ST_BLOCK;
            get=get->next_data;
          } else if(!get->data) { /* Numeric data, but it's Zero */
            data_copy[0] = '0'; data_copy[1] = EOL;
          } else { 
            memcpy(data_copy, get->data, (unsigned)get->datalen);
            data_copy += get->datalen; 
            *data_copy = EOL; data_copy++;
            get=NULL;
          }
        }
      }
      return; 
#else
	if ((i = svnaptr[(int) key[0]])) {
	    k = 1;
	    j = i + 1;			/* first char always matches! */
	    do {
		while (key[k] == svntable[++j]) {	/* compare keys */
		    if (key[k++] == EOL) {
			i = UNSIGN (svntable[++j]);
			stcpy0 (data, &svntable[j + 1], i);
			data[i] = EOL;
			return;
		    }
		}
		i += UNSIGN (svntable[i]);	/* skip key */
		i += UNSIGN (svntable[i]) + 1;	/* skip data */
		k = 0;
		j = i;
	    } while (i < UDFSVSIZ);
	}
	ierr = ILLFUN;
	return;
#endif

    case set_sym:			/* store/create variable; */
#ifdef EXPERIMENTAL_SETGET
        if(!UDFSV) { 
           UDFSV = NEW(subscript, 1);
#ifdef DEBUG_SYM
           printf("Created new UDFSV\r\n");
#endif
        }
        new_string = find_home(UDFSV, key);
#ifdef NUMERIC
        if(is_numeric(data)) {
          long_tmp = str2long(data);
          new_string->rc->numeric = long_tmp;
          if(new_string->rc && new_string->rc->reference) 
            new_string->rc->reference->rc->numeric = long_tmp;
          if(new_string->data) free(new_string->data);
          new_string->data = NULL;
        } else {
#endif
          new_string->rc->numeric = 0;
          if(new_string->rc && new_string->rc->reference) 
            new_string->rc->reference->rc->numeric = 0;
          data_copy = data;
          while(datal > ST_BLOCK) { 
            if(!new_string->data) 
              new_string->data = NEW(char,ST_BLOCK);
              
            if(!new_string->next_data)
              new_string->next_data = NEW(string,1);
            if(new_string->rc && new_string->rc->reference) 
              new_string->rc->reference->next_data = new_string->next_data;
            memcpy(new_string->data, data_copy, ST_BLOCK);
            new_string = new_string->next_data;
            datal -= ST_BLOCK; data_copy += ST_BLOCK;
          }
#ifdef DEBUG_SYM
          printf("[datal] datal is [%d]\r\n",datal);
#endif
          new_string->datalen = datal;
          if(new_string->rc && new_string->rc->reference) 
            new_string->rc->reference->datalen = datal;
          if(!new_string->data) {
#ifdef DEBUG_SYM
            printf("Alloc'd new memory for remaining string bit\r\n");
            printf("Datalen is (%d)\r\n",new_string->datalen);
#endif
            new_string->data = NEW(char,ST_BLOCK);
          }
          memcpy(new_string->data, data_copy, datal+1);
#ifdef DEBUG_SYM
        printf("[data] of last string seg is [");
        for(start = new_string->data; *start != EOL; start++)
          printf("%c",*start);
        printf("]\r\n");
        if(new_string->rc && new_string->rc->next_node) { 
          printf("The next node is [");
          for(start = new_string->rc->next_node->rc->key; *start != EOL; start++)
            printf("%c",*start);
          printf("]\r\n");
        } else printf("There is no next node.\r\n"); 
#endif
#ifdef NUMERIC
      }
#endif
        if(new_string->next_data) {  /* Extra memory can be liberated */
          tmp = new_string;
          new_string = new_string->next_data;
          tmp->next_data = NULL; /* Cut off the string */
          while(new_string != NULL) { /* Free the remaining data */
            free_string = new_string;
            new_string = new_string->next_data;
            free(free_string);
          }
        }
        return;
#else
	if ((keyl = stlen (key) + 2) > STRLEN) {
	    ierr = MXSTR;
	    return;
	}				/* key length +2 */
	datal = stlen (data);		/* data length */

	if ((i = svnaptr[(int) key[0]])) {	/* previous entry */
	    j = i + 1;
	    k = 1;
	} else {
	    i = svnlen;
	    j = i;
	    k = 0;
	}

	while (i < UDFSVSIZ) {		/* compare keys */
	    while (key[k] == svntable[++j]) {
		if (key[k] == EOL)
		    goto old;
		k++;
	    }
	    if (key[k] < svntable[j])
		break;
	    i += UNSIGN (svntable[i]);	/* skip key */
	    i += UNSIGN (svntable[i]) + 1;	/* skip data */
	    j = i;
	    k = 0;
	}
/* if    entry found,     i pointer to searched entry
 * else  entry not found, i pointer to alphabetically next entry */
/* new entry */
	k = i;
	j = key[0];
	i = keyl + datal + 1;
	if (svnlen <= i) {
	    long    dif;

	    dif = getumore ();
	    if (dif == 0L)
		return;
	    k += dif;
	}
	for (k1 = 'a'; k1 <= j; k1++) {
	    if (svnaptr[k1])
		svnaptr[k1] -= i;
	}
	i = k - i;
	if (svnaptr[j] == 0 || svnaptr[j] > i)
	    svnaptr[j] = i;
	i = (svnlen -= (j = keyl + datal + 1));
	stcpy0 (&svntable[i], &svntable[j + i], k - i);
	i = k - (keyl + datal + 1);
	svntable[i++] = (char) (keyl);
	stcpy (&svntable[i], key);	/* store new key */
	i += keyl - 1;

	svntable[i++] = (char) (datal);
	stcpy0 (&svntable[i], data, datal);	/* store new data */
	return;

/* there is a previous value */
      old:i += UNSIGN (svntable[i]);
	j = UNSIGN (svntable[i]) - datal;
	if (j < 0) {			/* more space needed */
	    if (svnlen <= (-j)) {
		long    dif;

		dif = getumore ();
		if (dif == 0L)
		    return;
		i += dif;
	    }
	    svnlen += j;
	    for (k = 'a'; k < key[0]; k++) {
		if (svnaptr[k])
		    svnaptr[k] += j;
	    }
	    if (svnaptr[k] && svnaptr[k] < i)
		svnaptr[k] += j;
	    k = i + j;
	    i = svnlen;
	    stcpy0 (&svntable[i], &svntable[i - j], k - i);
	    i = k;
	} else if (j > 0) {		/* surplus space */
	    svnlen += j;
	    for (k = 'a'; k < key[0]; k++) {
		if (svnaptr[k])
		    svnaptr[k] += j;
	    }
	    if (svnaptr[k] && svnaptr[k] < i)
		svnaptr[k] += j;
	    i += j;
	    k = i;
	    j = i - j;
	    while (i >= svnlen) {
		svntable[i--] = svntable[j--];
	    }
	    i = k;
	}
	svntable[i++] = (char) (datal);
	stcpy0 (&svntable[i], data, datal);	/* store new data */
	return;
/* end of set_sym section */
#endif
    }
}					/* end user defined special variable table */
/******************************************************************************/
long
getpmore ()
{					/* try to get more 'partition' space. returns size increment */
    long    siz,
            dif;

    if (autopsize == FALSE)
	return 0L;
    siz = PSIZE;
    if (siz % 1024)
	siz = (siz & ~01777) + 02000;	/* round for full kB; */
    siz += 01777;
    dif = siz - PSIZE;
    if (newpsize (siz))
	return 0L;
    return dif;
}					/* end getpmore */
/******************************************************************************/
long
getumore ()
{					/* try to get more udfsvntab space. returns size increment */
    long    siz,
            dif;

    if (autousize == FALSE) {
	ierr = STORE;
	return 0L;
    }
    siz = UDFSVSIZ;
    if (siz % 1024)
	siz = (siz & ~01777) + 02000;	/* round for full kB; */
    siz += 01777;
    dif = siz - UDFSVSIZ;
    if (newusize (siz)) {
	ierr = STORE;
	return 0L;
    }
    return dif;
}					/* end getumore */
/******************************************************************************/
long
getrmore ()
{					/* try to get more routine space. returns size increment */
    long    siz,
            dif;
    short   i;

    if (autorsize == FALSE) {
	ierr = PGMOV;
	return 0L;
    }
    siz = PSIZE0;
    if (siz % 1024)
	siz = (siz & ~01777) + 02000;	/* round for full kB; */
    siz += 01777;
    dif = siz - PSIZE0;
    for (i = 0; i < NO_OF_RBUF; i++) {	/* empty routine buffer */
	pgms[i][0] = EOL;
	ages[i] = 0L;
    }
    if (newrsize (siz, NO_OF_RBUF)) {
	ierr = PGMOV;
	return 0L;
    }
    return dif;
}					/* end getrmore */
/******************************************************************************/
/* #ifndef NEWSTACK */ /* Someday we won't need this function */
short int
getnewmore ()
{					/* enlarge new_buffers */
    char   *newbuf;
    int     i;
    long    dif;

    newbuf = calloc ((unsigned) (NSIZE + 4096), 1);	/* new_buffer                      */
    if (newbuf == NULL) {		/* could not allocate stuff...     */
	ierr = STKOV;
	return 1;
    }
    stcpy0 (newbuf, newstack, (long) NSIZE);
    dif = newbuf - newstack;
    free (newstack);			/* free previously allocated space */
    newstack = newbuf;
    NSIZE += 4096;
    newptr += dif;
    newlimit = newstack + NSIZE - 1024;
    i = 0;
    while (i <= nstx) {
	if (nestnew[i])
	    nestnew[i] += dif;
	i++;
    }
    return 0;
}					/* end getnewmore() */
/* #endif */
/******************************************************************************/

#ifdef NEW_WAY
/* Find somewhere to put a new string */
string *find_home(subscript *subs, char *key) {
  int i; char *tmp = NEW(char,stlen(key)+1);
  string *find = NULL, *last = NULL, *new = NULL, *new_sub = NULL;
  int loop;
  for(i=0; (*key != DELIM) && (*key != EOL); key++, i++) tmp[i] = *key;
  tmp[i] = EOL;
  if(!subs->index[tmp[0]-32]) { /* No Alpha Entry */
    new = NEW(string,1);
    new->rc = NEW(string_rc,1);
    new->rc->next_node = (string *)NULL; 
    subs->index[tmp[0]-32] = new;
    /* New Entry created.  Needs to be setup and subscripted */
#ifdef DEBUG_SYM
     printf("No Alpha Entry.  Created one.\r\n");
#endif
  } else {
    for(find = subs->index[tmp[0]-32];
        find && (stcmp(tmp, find->rc->key) > 0);
        find = find->rc->next_node) last = find;
    if(!last && stcmp(tmp,find->rc->key)) { /* Our key comes first */
#ifdef DEBUG_SYM
     printf("Alpha Entry, but our new key comes first.\r\n");
     printf("Find: (%d) Last: (%d) Subs->Index: (%d)\r\n",
            find, last, subs->index[tmp[0]-32]);
     printf("Our key [");
     for(i=0; tmp[i] != EOL; i++) printf("%c",tmp[i]);
     printf("]\r\nNext key is [");
     for(i=0; find->rc->key[i] != EOL; i++) printf("%c",find->rc->key[i]);
     printf("]\r\n");
#endif
      new = NEW(string,1);
      new->rc = NEW(string_rc, 1);
      new->rc->next_node = subs->index[tmp[0]-32];
      subs->index[tmp[0]-32] = new;
    } else if(!find || stcmp(tmp,find->rc->key)) { /* No Old Key */
      new = NEW(string,1);
      new->rc = NEW(string_rc,1);
      last->rc->next_node = new; new->rc->next_node = find;
#ifdef DEBUG_SYM
     printf("Inserted our key in the middle of a list.\r\n");
#endif
      /* Made new entry and linked it.  Needs to be setup and subscripted */
    } else { /* Key matched.  Check for subscripting */
      if(*key == DELIM) { /* We're subscripting */
        key++; 
        if(!find->rc) {
           find->rc = NEW(string_rc,1);
        }
        if(!find->rc->subs) find->rc->subs = NEW(subscript,1);
        if(!find->rc->key) { 
          find->rc->key = NEW(char,stlen(tmp)+1);
          stcpy(find->rc->key, tmp);
        }
#ifdef DEBUG_SYM
     printf("Key match.  Next round of Subscripts.\r\n");
#endif
        return find_home(find->rc->subs, key); 
        /* return something ready to have data stored in it */
      } else { 
#ifdef DEBUG_SYM
     printf("Exact Key Match.  Party on Garth!\r\n");
#endif
        return find; /* Exact match */ 
      }
    }
  }
  if(*key == EOL) {
    if(!new->rc) new->rc = NEW(string_rc,1);
    new->rc->key = NEW(char,stlen(tmp)+1);
    stcpy(new->rc->key,tmp);
  } else while(*key == DELIM) {
    /* We need to setup subscripts for a new string */
    if(!new->rc) new->rc = NEW(string_rc,1);
    new->rc->key = NEW(char,stlen(tmp)+1);
    stcpy(new->rc->key,tmp);
    key++; for (i=0; *key != DELIM && *key != EOL; key++, i++) tmp[i] = *key;
    tmp[i] = EOL;
    new->rc->subs = NEW(subscript,1);
    new->rc->subs->index[tmp[0]-32] = NEW(string,1);
    new = new->rc->subs->index[tmp[0]-32]; 
  }
  if(!new->rc) new->rc = NEW(string_rc,1);
  new->rc->key = NEW(char,stlen(tmp)+1);
  stcpy(new->rc->key, tmp);
  new->rc->subs = NULL;  new->next_data = NULL;
  return new;  /* ready for data */
}

#ifdef NEWSTACK
/* Return a list of branches traveled to find a given string */
locator *seek_leaf(subscript *subs, char *key, locator *parent) {
   locator *this_level, *next_level;
   int z; char *tmp_key = NEW(char,stlen(key)+1);
   string *find, **last = NULL;
   if(!subs || !subs->index[key[0]-32]) return NULL; 
   else {
     find = subs->index[key[0]-32]; last = &subs->index[key[0]-32];
     for(z=0; *key != DELIM && *key != EOL; key++, z++) tmp_key[z] = *key;
     tmp_key[z] = EOL;
     while(find && (stcmp(tmp_key, find->rc->key) > 0)) { 
       last = &find->rc->next_node; find = find->rc->next_node; 
     }
     if(!find || (stcmp(tmp_key, find->rc->key) > 0)) return NULL; 

     this_level = NEW(locator,1);
     this_level->parent = parent;
     this_level->branch = find;
     if(*key==DELIM) { 
       this_level->child = seek_leaf(find->rc->subs, key+1, this_level);
       if(!this_level->child) { free(this_level); return NULL; }
     } else this_level->child = NULL;
     return this_level;
   }
}

/* Gets immediately previous entry */
string *get_previous(string *target, locator *parent) {
  int i; subscript *working; char start_letter; string *seek;
  string *start, *stop;
  if(!parent) return NULL;
  working = parent->branch->rc->subs; /* Work with all parent's subscripts */
  start_letter = target->rc->key[0]-32; /* First letter of target key */
  if(working->index[start_letter] != target) {
    seek = working->index[start_letter];
    while(seek && (seek->rc->next_node != target)) seek = seek->rc->next_node;
    return seek;
  }
  for(i=start_letter-1; i>-1; i--) 
    if(working->index[i]) { 
      seek = working->index[i];
      while(seek->rc->next_node != NULL) seek = seek->rc->next_node;
      return seek;
    }
  return NULL; /* There is no previous entry on this level */
}

/* Return immediately next string */
string *get_next(string *target, locator *parent) {
  int i; subscript *working; char start_letter;
  if(target->rc->next_node) return target->rc->next_node;
  working = parent->branch->rc->subs; /* Work with all parent's subscripts */
  start_letter = target->rc->key[0]-32; /* First letter of target key */
  for(i=start_letter+1; i<96; i++) 
    if(working->index[i]) return working->index[i];
  return NULL;
}

/* Returns the previous/next string with data in relation to the one given.
 * If the string is the first/last, returns NULL.
 * Returns next if the variable next is nonzero, previous otherwise
 */
/** UNUSED **/
string *string_along(string *target, locator *parent, int next) {
  int i; subscript *working; char start_letter; string *seek;
  string *start, *stop;
  /* Piece of cake ;) */
  if(next && target->rc->next_node) return target->rc->next_node;
  if(!parent) return NULL;
  working = parent->branch->rc->subs; /* Work with all parent's subscripts */
  start_letter = target->rc->key[0]-32; /* First letter of target key */
  if(next) {
    for(i=start_letter+1; i<96; i++) 
      if(working->index[i]) {
        seek = working->index[i];
        while(seek && !DATA(seek)) { seek = seek->rc->next_node; } 
        if(seek) return seek;
      }
  } else {
    if(working->index[start_letter] != target) {
      seek = get_last_datanode(working->index[start_letter], target);
      if(seek) return seek;
    }
    for(i=start_letter-1; i>-1; i--) 
      if(working->index[i]) { 
        seek = get_last_datanode(working->index[i], NULL);
        if(seek) return seek;
      }
  }  
  return NULL; 
}

/* Returns either the first or the last string in a subscript;
 * if "first" is zero, returns the last element, otherwise 
 * returns the first.
 * Returns null if the subscript doesn't exist, or has no elements...
 */
string *string_end(subscript *start, int first) {
  char i;
  string *seek, *stop;
  if(!start) return NULL;
#ifdef DEBUG_SYM
  printf("We are looking for the %s element...",(first) ? "first" : "last" );
#endif
  if(first) { 
    for(i=0; i<96; i++) if(start->index[i]) {
       return start->index[i];
    }
  }
  else { 
    for(i=95; i>-1; i--) 
      if(start->index[i]) { 
        seek = start->index[i];
        while (seek->rc->next_node) seek = seek->rc->next_node;
        if(seek) return seek;
      }
  }
  return NULL;
}
#endif /* NEWSTACK */

/* Given a starting subscript, find string matching key */
/* If last is non-null, sets to address of previous node */
string *seek_string(subscript *subs, char *key, string ***last_node) {
   int z; char *tmp_key = NEW(char,stlen(key)+1);
   string *find, **last = NULL;
   if(!subs || !subs->index[key[0]-32]) { 
#ifdef DEBUG_SYM
     printf("Didn't get subs, or no subs->index.\r\n");
#endif
     return NULL; 
   }
   else {
     find = subs->index[key[0]-32]; last = &subs->index[key[0]-32];
     for(z=0; *key != DELIM && *key != EOL; key++, z++) tmp_key[z] = *key;
     tmp_key[z] = EOL;
#ifdef DEBUG_SYM
       printf("starting: tmp_key ["); 
       for(z=0; tmp_key[z] != EOL; z++) printf("%c",tmp_key[z]);
       printf("] to find->key [");
       for(z=0; find->rc->key[z] != EOL; z++) printf("%c",find->rc->key[z]);
       printf("] (%d) {%d}\r\n",find, stcmp(tmp_key, find->rc->key));
#endif
     while(find && (stcmp(tmp_key, find->rc->key) > 0)) { 
#ifdef DEBUG_SYM
       printf("comparing tmp_key ["); 
       for(z=0; tmp_key[z] != EOL; z++) printf("%c",tmp_key[z]);
       printf("] to find->rc->key [");
       for(z=0; find->rc->key[z] != EOL; z++) printf("%c",find->rc->key[z]);
       printf("]\r\n");
#endif
       last = &find->rc->next_node; find = find->rc->next_node; 
     }
     if(!find || (stcmp(tmp_key, find->rc->key) > 0)) { 
#ifdef DEBUG_SYM
       printf("Went past where it should have been.\r\n");
#endif
       return NULL; 
     }
     else if(*key==DELIM) return seek_string(find->rc->subs, key+1, last_node);
     else { 
       if(last_node) *last_node = last;  
       return find;
     }
   }
}

/* These better be Alloc'd first! */
void copy_string(string *dest, string *src) {
  string *tmp, *new_block, *last_block;
  if(!src || !dest) { return; }
  dest->datalen = src->datalen;
  dest->data = NEW(char,ST_BLOCK);
  memcpy(dest->data, src->data,ST_BLOCK);
  last_block = dest;
  for(tmp = src->next_data; tmp; tmp = tmp->next_data) { 
    if(last_block->next_data) { 
      new_block  = last_block->next_data;
    } else {
      new_block = NEW(string,1);
      new_block->data = NEW(char,ST_BLOCK);
      last_block->next_data = new_block; 
    }
    memcpy(new_block->data, tmp->data,ST_BLOCK);
    new_block->datalen = tmp->datalen;
    new_block->next_data = NULL;
    last_block = new_block;
  }
  if(!dest->rc) dest->rc = NEW(string_rc,1);
  if(src->rc) { 
    dest->rc->reference = src->rc->reference;
    if(src->rc->subs) {
      if(!dest->rc->subs) 
        dest->rc->subs = NEW(subscript,1);
      copy_subscript(dest->rc->subs, src->rc->subs);
    }
    /* Don't copy the key or the next-node pointer */
  } 
}


/* Iterates through a subscript and copies one to another */
void copy_subscript(subscript *dest, subscript *src) {
  int loop;
  for(loop=0; loop<96; loop++) {
    if(src->index[loop]) 
      dest->index[loop] = copy_list(dest->index[loop], src->index[loop]);
  }
}

/* Returns a pointer to start of list */
string *copy_list(string *dest, string *src) {
   string *dup_head = NULL, *new, *last = NULL, *find = dest, *tmp = src;
   if(dest && (stcmp(dest->rc->key, src->rc->key) < 0)) dup_head = dest;
   while(tmp) {
     new = NEW(string,1);
     copy_string(new, tmp);
     new->rc->key = NEW(char,stlen(tmp->rc->key)+1);
     stcpy(new->rc->key, tmp->rc->key);
     if(!dup_head) dup_head = new; /* First element */
     else { 
       while(find && stcmp(find->rc->key, new->rc->key) >= 0) {
         last = find; find = find->rc->next_node;
       }
       if(find && !stcmp(find->rc->key, new->rc->key)) {
         new->rc->next_node = find->rc->next_node;
         if(find->rc->subs) { 
           new->rc->subs  = find->rc->subs;
           find->rc->subs = NULL;
         }
         kill_string(find);
       } else new->rc->next_node = find;
     }
     if(last) last->rc->next_node = new;
     last = new;
     tmp = tmp->rc->next_node;
   }
   last->rc->next_node = NULL;
   return dup_head;
}
     
   
   
          
/* Fries a string and all subscripts - better link around it first! */
int kill_string(string *fry) {
  string *last, *seek;

  /* Don't trash the original if using a reference */
  if(fry->rc && (fry->rc->reference)) {
#ifdef DEBUG_SYM
    printf("Not gonna free a reference... (%d)\r\n",fry->data);
#endif
    free(fry->rc); return;
  }

  /* First free all the string data */
  free(fry->data); seek = fry->next_data;
  while(seek) { 
    last = seek; seek = seek->next_data; free(last->data); free(last); 
  }
 
  if(fry->rc) { 
    /* Free all subscripted data */
    if(fry->rc->subs) { kill_subscript(fry->rc->subs); free(fry->rc->subs); }

    /* Free the key */
    if(fry->rc->key) free(fry->rc->key);
    free(fry->rc); 
  }
  return 0;
}

/* Kill off a subscript (de-allocate it later if necessary) */
int kill_subscript(subscript *subs) {
   int i;
   string *last, *seek;
   for(i=0; i<96; i++) if(subs->index[i]) {
       seek = subs->index[i]->rc->next_node;
       kill_string(subs->index[i]);
       while(seek) { 
         last = seek; seek = seek->rc->next_node; kill_string(last);
       }
       subs->index[i] = NULL;
   }
   return 0;
}

#ifdef NEWSTACK 
/* Return the last string in a list with data in it */
string *get_last_datanode(string *start, string *stop) {
  string *result;
  if(!start) return NULL;
  else if(start->rc->next_node == stop) {
    if(DATA(start)) return start; else return NULL;
  } else result = get_last_datanode(start->rc->next_node, stop);
  if(result) return result;
  else if(DATA(start)) return start; 
  else return NULL;
}

/* Return the first string under a subscript */
locator *get_first_string(subscript *start, string **get, locator *parent) {
  char i; locator *this_level;
  string *tmp, *result;
  if(!start) { *get = NULL; return NULL; }
  this_level = NEW(locator,1);
  this_level->parent = parent;
  for(i=0; i<96; i++) 
    if(start->index[i]) {
      tmp = start->index[i];
      if(DATA(tmp)) { 
        this_level->branch = tmp; this_level->child = NULL; *get = tmp; 
        return NULL; 
      }
      while(tmp) {
        this_level->child=get_first_string(tmp->rc->subs, &result, this_level);
        if(result) { this_level->branch=tmp; *get=result; return this_level; } 
        else tmp = tmp->rc->next_node;
      }
    }
  free(this_level); *get = NULL; return;
}

/* Return the last string under a subscript */
locator *get_last_string(subscript *start, string **get, locator *parent) {
  char i; locator *this_level;
  string *tmp, *result;
  string *beg, *end;
  if(!start) { *get = NULL; return NULL; }
  this_level = NEW(locator,1);
  this_level->parent = parent;
  for(i=95; i>-1; i--)
    if(start->index[i]) {
      beg = get_last_datanode(start->index[i], NULL);
      if(!beg) beg = start->index[i];
      end = NULL;
      while(beg != end) {
        tmp = beg; 
        while(tmp->rc->next_node != end) tmp = tmp->rc->next_node;
        if(tmp->rc->subs) {
          this_level->child=get_last_string(tmp->rc->subs, &result, this_level);
          if(result) { 
            this_level->branch = tmp; *get = result; return this_level; 
          }
        } 
        if(DATA(tmp)) { 
          this_level->branch=tmp; *get = tmp; return this_level; 
          this_level->child=NULL;
        }
        else end = tmp;
      }
    }
  free(this_level); *get = NULL; return NULL;
}

void count_subnodes(subscript *count, counter *this_level) {
  counter *next_level = this_level->next;
  int i; string *tmp;
  /* Iterate through the subscript */
  for(i=0;i<96;i++) if(count->index[i]) {
    tmp = count->index[i];
    while(tmp) {
      if(DATA(tmp)) this_level->datanodes++;
      if(tmp->rc->subs) {
        if(!next_level) {
          next_level = NEW(counter, 1);
          next_level->datanodes = 0;
          next_level->next = NULL;
          this_level->next = next_level;
        }
        count_subnodes(tmp->rc->subs, next_level);
      }
      tmp = tmp->rc->next_node;
    }
  }
}
#endif

/* Returns 1 if the value is a qualifying integer within range of a 'long' */
int is_numeric(char *value) {
  int loop; char *check = value;
  char *range = "2147483647";
  if(!value) return 0;
  else if(value[0] == '-') check++; /* Potentially a negative number */
  if(check[0] == EOL || (check[0] == '0' && check[1] != EOL)) return 0; 
  for(loop=0;loop<10;loop++) /* A 'long' can store a number in the billions */
    if(check[loop] == EOL) return 1;
    else if(check[loop] < '0' || check[loop] > '9') return 0;
  if(check[loop] != EOL) return 0;
  /* A ten-digit number, check if it's in range */
  loop=0;
  while(range[loop]) {
    if(check[loop] < range[loop]) return 1;
    if(check[loop] > range[loop]) return 0;
    loop++;
  }
  return 1;
}
#endif NEW_WAY /* New string functions */

/* End of $Source: /cvsroot-fuse/gump/FreeM/src/symtab.c,v $ */
