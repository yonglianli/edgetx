/*
 ** $Id: luac.c,v 1.76 2018/06/19 01:32:02 lhf Exp $
 ** Lua compiler (saves bytecodes to files; also lists bytecodes)
 ** See Copyright Notice in lua.h
 */

#define luac_c
#define LUA_CORE

#include "lprefix.h"

#include <alloca.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "ldebug.h"
#include "lnodemcu.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "lundump.h"
#include "lopcodes.h"

static void PrintFunction(const Proto* f, int full);
#define luaU_print	PrintFunction

#define PROGNAME        "luac.cross"	 /* default program name */
#define OUTPUT          PROGNAME ".luac" /* default output file */

static int listing=0;			/* list bytecodes? */
static int dumping=1;			/* dump bytecodes? */
static int stripping=0;			/* strip debug information? */
static char Output[]={ OUTPUT };	/* default output file name */
static const char* output=Output;	/* actual output file name */
static const char* progname=PROGNAME;	/* actual program name */
static int lookup = 0;			/* output lookup-style master combination header */

static void fatal(const char *message) {
  fprintf(stderr, "%s: %s\n", progname, message);
  exit(EXIT_FAILURE);
}

static void cannot(const char *what) {
  fprintf(stderr, "%s: cannot %s %s: %s\n", progname, what, output, strerror(errno));
  exit(EXIT_FAILURE);
}

static void usage(const char *message) {
  if ( *message == '-')
    fprintf(stderr, "%s: unrecognized option '%s'\n", progname, message);
  else
    fprintf(stderr, "%s: %s\n", progname, message);
  fprintf(stderr,
    "usage: %s [options] [filenames]\n"
    "Available options are:\n"
    "  -l       list (use -l -l for full listing)\n"
    "  -o name  output to file 'name' (default is \"%s\")\n"
    "  -i       generate lookup combination master (default with option -f)\n"
    "  -m size  maximum LFS image in bytes\n"
    "  -p       parse only\n"
    "  -s       strip debug information\n"
    "  -v       show version information\n"
    "  --       stop handling options\n"
    "  -        stop handling options and process stdin\n", progname, Output);
  exit(EXIT_FAILURE);
}

#define IS(s)	(strcmp(argv[i],s)==0)

static int doargs(int argc, char *argv[]) {
  int i;
  int version = 0;
  lu_int32 offset = 0;
  if (argv[0] != NULL && *argv[0] != 0) progname = argv[0];
  for (i = 1; i < argc; i++) {
    if ( *argv[i] != '-') {                       /* end of options; keep it */
      break;
    } else if (IS("--")) {                         /* end of options; skip it */
      ++i;
      if (version) ++version;
      break;
    } else if (IS("-")) {                        /* end of options; use stdin */
      break;
    } else if (IS("-i")) {                                          /* lookup */
      lookup = 1;
    }  else if (IS("-l")) {                                           /* list */
      ++listing;
    } else if (IS("-o")) {                                     /* output file */
      output = argv[++i];
      if (output == NULL || *output == 0 || ( *output == '-' && output[1] != 0))
        usage("'-o' needs argument");
      if (IS("-")) output = NULL;
    } else if (IS("-p")) {                                      /* parse only */
      dumping = 0;
    } else if (IS("-s")) {                         /* strip debug information */
      stripping = 1;
    } else if (IS("-v")) {                                    /* show version */
      ++version;
    } else {                                                /* unknown option */
      usage(argv[i]);
    }
  }

  if (i == argc && (listing || !dumping)) {
    dumping = 0;
    argv[--i] = Output;
  }

  if (version) {
    printf("%s\n", LUA_COPYRIGHT);
    if (version == argc - 1) exit(EXIT_SUCCESS);
  }
  return i;
}


static int writer(lua_State *L, const void *p, size_t size, void *u) {
  UNUSED(L);
  return (fwrite(p, size, 1, ((FILE **)u)[0]) != 1) && (size != 0);
}


static int msghandler (lua_State *L) {
  const char *msg = lua_tostring(L, 1);
  if (msg == NULL)  /* is error object not a string? */
    msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
  luaL_traceback(L, L, msg, 1);  /* append a standard traceback */
  return 1;  /* return the traceback */
}


static int dofile (lua_State *L, const char *name) {
  int status = luaL_loadfile(L, name);
  if (status == LUA_OK) {
    int base = lua_gettop(L);
    lua_pushcfunction(L, msghandler);  /* push message handler */
    lua_insert(L, base);  /* put it under function and args */
    status = lua_pcall(L, 0, 0, base);
    lua_remove(L, base);  /* remove message handler from the stack */
  }
  if (status != LUA_OK) {
    fprintf(stderr, "%s: %s\n", PROGNAME, lua_tostring(L, -1));
    lua_pop(L, 1);  /* remove message */
  }
  return status;
}

static int pmain(lua_State *L) {
  int argc = (int) lua_tointeger(L, 1);
  char **argv = (char **) lua_touserdata(L, 2);
  char **filelist = alloca(argc * sizeof(char *));
  const Proto *f;
  int i, status;
  if (!lua_checkstack(L, argc + 1))
    fatal("too many input files");
  if (argc == 0)
    return 0;
  for (i = 0; i < argc; i++) {
    const char *filename = IS("-") ? NULL : filelist[i];
    if (luaL_loadfile(L, filename) != LUA_OK)
      fatal(lua_tostring(L, -1));
  }
  f = getproto(L->top - 1);
  if (listing) luaU_print(f, listing > 1);
  if (dumping) {
    int result;
    FILE *D = (output == NULL) ? stdout : fopen(output, "wb");
    if (D == NULL) cannot("open");
    lua_lock(L);
    result = luaU_dump(L, f, writer, cast(void *, &D), stripping);
    lua_unlock(L);
    if (ferror(D)) cannot("write");
    if (fclose(D)) cannot("close");
  }
  return 0;
}

int main(int argc, char *argv[]) {
  lua_State *L;
  int i = doargs(argc, argv);
  int j, status;
  argc -= i; argv += i;
  if (argc <= 0) usage("no input files given");
  for (j = 0; j < 2 ; j++) {
    L = luaL_newstate();
    if (L == NULL) fatal("not enough memory for state");
    lua_pushcfunction(L, &pmain);
    lua_pushinteger(L, argc);
    lua_pushlightuserdata(L, argv);
    status = lua_pcall(L, 2, 0, 0);
    if (status != LUA_OK) {
      if (lua_isboolean(L,-1) && lua_toboolean(L,-1)) {
        /* simulate a restart */
        lua_close(L);
        continue; /* and loop around once more simulating restart */
      }
      char *err = strdup(lua_tostring(L, -1));
      lua_close(L);
      fatal(err);
    }
    lua_close(L);
    break;
  }
  return EXIT_SUCCESS;
}


static void PrintString(const TString* ts)
{
 const char* s=getstr(ts);
 size_t i,n=tsslen(ts);
 printf("%c",'"');
 for (i=0; i<n; i++)
 {
  int c=(int)(unsigned char)s[i];
  switch (c)
  {
   case '"':  printf("\\\""); break;
   case '\\': printf("\\\\"); break;
   case '\a': printf("\\a"); break;
   case '\b': printf("\\b"); break;
   case '\f': printf("\\f"); break;
   case '\n': printf("\\n"); break;
   case '\r': printf("\\r"); break;
   case '\t': printf("\\t"); break;
   case '\v': printf("\\v"); break;
   default:	if (isprint(c))
   			printf("%c",c);
		else
			printf("\\%03d",c);
  }
 }
 printf("%c",'"');
}

static void PrintConstant(const Proto* f, int i)
{
 const TValue* o=&f->k[i];
 switch (ttype(o))
 {
  case LUA_TNIL:
	printf("nil");
	break;
  case LUA_TBOOLEAN:
	printf(bvalue(o) ? "true" : "false");
	break;
  case LUA_TNUMFLT:
	{
	char buff[100];
	sprintf(buff,LUA_NUMBER_FMT,fltvalue(o));
	printf("%s",buff);
	if (buff[strspn(buff,"-0123456789")]=='\0') printf(".0");
	break;
	}
  case LUA_TNUMINT:
	printf(LUA_INTEGER_FMT,ivalue(o));
	break;
  case LUA_TSHRSTR: case LUA_TLNGSTR:
	PrintString(tsvalue(o));
	break;
  default:				/* cannot happen */
	printf("? type=%d",ttype(o));
	break;
 }
}

#define VOID(p)	     ((const void*)(p))
#define UPVALNAME(x) ((f->upvalues[x].name) ? getstr(f->upvalues[x].name) : "-")
#define MYK(x)	     (-1-(x))


static void PrintCode(const Proto* f)
{
 const Instruction* code=f->code;
 int pc,n=f->sizecode;
 for (pc=0; pc<n; pc++)
 {
  Instruction i=code[pc];
  OpCode o=GET_OPCODE(i);
  int a=GETARG_A(i);
  int b=GETARG_B(i);
  int c=GETARG_C(i);
  int ax=GETARG_Ax(i);
  int bx=GETARG_Bx(i);
  int sbx=GETARG_sBx(i);
  int line=getfuncline(f,pc);
  printf("\t%d\t",pc+1);
  if (line>0) printf("[%d]\t",line); else printf("[-]\t");
  printf("%-9s\t",luaP_opnames[o]);
  switch (getOpMode(o))
  {
   case iABC:
    printf("%d",a);
    if (getBMode(o)!=OpArgN) printf(" %d",ISK(b) ? (MYK(INDEXK(b))) : b);
    if (getCMode(o)!=OpArgN) printf(" %d",ISK(c) ? (MYK(INDEXK(c))) : c);
    break;
   case iABx:
    printf("%d",a);
    if (getBMode(o)==OpArgK) printf(" %d",MYK(bx));
    if (getBMode(o)==OpArgU) printf(" %d",bx);
    break;
   case iAsBx:
    printf("%d %d",a,sbx);
    break;
   case iAx:
    printf("%d",MYK(ax));
    break;
  }
  switch (o)
  {
   case OP_LOADK:
    printf("\t; "); PrintConstant(f,bx);
    break;
   case OP_GETUPVAL:
   case OP_SETUPVAL:
    printf("\t; %s",UPVALNAME(b));
    break;
   case OP_GETTABUP:
    printf("\t; %s",UPVALNAME(b));
    if (ISK(c)) { printf(" "); PrintConstant(f,INDEXK(c)); }
    break;
   case OP_SETTABUP:
    printf("\t; %s",UPVALNAME(a));
    if (ISK(b)) { printf(" "); PrintConstant(f,INDEXK(b)); }
    if (ISK(c)) { printf(" "); PrintConstant(f,INDEXK(c)); }
    break;
   case OP_GETTABLE:
   case OP_SELF:
    if (ISK(c)) { printf("\t; "); PrintConstant(f,INDEXK(c)); }
    break;
   case OP_SETTABLE:
   case OP_ADD:
   case OP_SUB:
   case OP_MUL:
   case OP_MOD:
   case OP_POW:
   case OP_DIV:
   case OP_IDIV:
   case OP_BAND:
   case OP_BOR:
   case OP_BXOR:
   case OP_SHL:
   case OP_SHR:
   case OP_EQ:
   case OP_LT:
   case OP_LE:
    if (ISK(b) || ISK(c))
    {
     printf("\t; ");
     if (ISK(b)) PrintConstant(f,INDEXK(b)); else printf("-");
     printf(" ");
     if (ISK(c)) PrintConstant(f,INDEXK(c)); else printf("-");
    }
    break;
   case OP_JMP:
   case OP_FORLOOP:
   case OP_FORPREP:
   case OP_TFORLOOP:
    printf("\t; to %d",sbx+pc+2);
    break;
   case OP_CLOSURE:
    printf("\t; %p",VOID(f->p[bx]));
    break;
   case OP_SETLIST:
    if (c==0) printf("\t; %d",(int)code[++pc]); else printf("\t; %d",c);
    break;
   case OP_EXTRAARG:
    printf("\t; "); PrintConstant(f,ax);
    break;
   default:
    break;
  }
  printf("\n");
 }
}

#define SS(x)	((x==1)?"":"s")
#define S(x)	(int)(x),SS(x)

static void PrintHeader(const Proto* f)
{
 const char* s=f->source ? getstr(f->source) : "=?";
 if (*s=='@' || *s=='=')
  s++;
 else if (*s==LUA_SIGNATURE[0])
  s="(bstring)";
 else
  s="(string)";
 printf("\n%s <%s:%d,%d> (%d instruction%s at %p)\n",
 	(f->linedefined==0)?"main":"function",s,
	f->linedefined,f->lastlinedefined,
	S(f->sizecode),VOID(f));
 printf("%d%s param%s, %d slot%s, %d upvalue%s, ",
	(int)(f->numparams),f->is_vararg?"+":"",SS(f->numparams),
	S(f->maxstacksize),S(f->sizeupvalues));
 printf("%d local%s, %d constant%s, %d function%s\n",
	S(f->sizelocvars),S(f->sizek),S(f->sizep));
}

static void PrintDebug(const Proto* f)
{
 int i,n;
 n=f->sizek;
 printf("constants (%d) for %p:\n",n,VOID(f));
 for (i=0; i<n; i++)
 {
  printf("\t%d\t",i+1);
  PrintConstant(f,i);
  printf("\n");
 }
 n=f->sizelocvars;
 printf("locals (%d) for %p:\n",n,VOID(f));
 for (i=0; i<n; i++)
 {
  printf("\t%d\t%s\t%d\t%d\n",
  i,getstr(f->locvars[i].varname),f->locvars[i].startpc+1,f->locvars[i].endpc+1);
 }
 n=f->sizeupvalues;
 printf("upvalues (%d) for %p:\n",n,VOID(f));
 for (i=0; i<n; i++)
 {
  printf("\t%d\t%s\t%d\t%d\n",
  i,UPVALNAME(i),f->upvalues[i].instack,f->upvalues[i].idx);
 }
}

static void PrintFunction(const Proto* f, int full)
{
 int i,n=f->sizep;
 PrintHeader(f);
 PrintCode(f);
 if (full) PrintDebug(f);
 for (i=0; i<n; i++) PrintFunction(f->p[i],full);
}
