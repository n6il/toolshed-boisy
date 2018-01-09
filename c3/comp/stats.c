/*
     Modification History:

     29-Aug-83      Improved while() code.
     29-Aug-83      Improved for() code.
     12-Sep-83 LAC  added register contents tracking code
     13-Apr-84 LAC  Conversion for UNIX.
*/

#include "cj.h"

/*******************************
 *                             *
 *   statement parser          *
 *                             *
 *******************************/

static swcherr();

typedef struct casestct {
    struct casestct *clink;     /* next case in case list */
    int cval;                   /* case value */
    short clab;                 /* case label */
} casnode;

#define CASESIZE    sizeof(casnode)

direct  casnode *caseptr,   /* list of cases */
                *lastcase,  /* last case in current list */
                *freecase;  /* list of spare case nodes */

#if 0
static int  swflag,         /* current switch level */
#else
static int
#endif
            deflabel;       /* switch default label */

static labstruc *breakptr,      /* break label */
                *contptr;       /* continue label */

extern  expnode *dregcont,      /* D register content */
                *xregcont;      /* X register content */

static  expnode *emit(), *getptest(), *gettest(), *sidexp();

static  symnode *checklabel();

statement()
{
   if (sym!=SEMICOL) lastst=0;

contin:

    switch (sym) {
        case SEMICOL: break;
        case KEYWORD:
            switch (symval) {
                case IF:        doif();         return;
                case WHILE:     dowhile();      return;
                case RETURN:    doreturn();     break;
                case CASE:      docase();       return;
                case SWITCH:    doswitch();     return;
                case BREAK:     dobreak();      break;
                case CONTIN:    docont();       break;
                case DEFAULT:   dodefault();    return;
                case FOR:       dofor();        return;
                case DO:        dodo();         break;
                case GOTO:      dogoto();       break;
                case ELSE:
                    error("no 'if' for 'else'");
                    getsym();
                    goto contin;
                default:        goto what;
            }
            break;
        case LBRACE:
            block(0);
            getsym();
            return;
        case 0xFFFF:
            return;   /* EOF */
        case NAME:
            blanks();
            if (cc==':') {
                dolabel();
                goto contin;
            }
            goto tryexp;
        default:
what:
            if (issclass() || istype()) {
                error("illegal declaration");
                do {getsym();} while(sym!=SEMICOL);
                break;
            } else {
tryexp:
                if (sidexp(0)==0) {
                    error("syntax error");
                    junk();
                    return;
                }
            }
    }
    need(SEMICOL);
}

doif()
{
    register expnode *ptr;
    labstruc tl,fl;
    register int et;

    getsym();
    ptr = getptest();

    tl.labnum = fl.labnum = 0;
    tl.labdreg = tl.labxreg = fl.labdreg = fl.labxreg = NULL;

    if (et = (sym == SEMICOL)) {
        getsym();
        dotest(ptr,&tl,&fl,FALSE);
        uselabel(&fl);
    } else {
        dotest(ptr,&tl,&fl,TRUE) ;
        uselabel(&tl);
        statement();
    }
    if (sym==KEYWORD && symval==ELSE) {
        getsym();
        if (sym != SEMICOL) {
            if (et == 0) {
                et = 1;
                gen(JMP,tl.labnum = getlabel(),0);
                tl.labdreg = dregcont;
                tl.labxreg = xregcont;
                dregcont = treecopy(fl.labdreg);
                xregcont = treecopy(fl.labxreg);
                uselabel(&fl);
            }
            statement();
        }
    }
    uselabel(et ? &tl : &fl);
}

dowhile()
{
    register expnode *ptr;
    labstruc brk,cnt,*savbreak,*savcont;
    register int tlab;

    savbreak = breakptr;
    savcont = contptr;
    breakptr = &brk;
    contptr = &cnt;

    getsym();

    brk.labnum =  0;
    brk.labdreg = brk.labxreg = NULL;
    cnt.labnum = getlabel();
    cnt.labdreg = dregcont;
    cnt.labxreg = xregcont;
    dregcont = xregcont = NULL;
    brk.labsp = cnt.labsp = sp;

    ptr = getptest();
    if (sym == SEMICOL)
        tlab = cnt.labnum;
    else {
        gen(JMP,cnt.labnum,0);
        label(tlab = getlabel());
        statement();
    }

    uselabel(&cnt);
    cnt.labnum = tlab;
    dotest(ptr,&cnt,&brk,FALSE);
    uselabel(&brk);

    breakptr = savbreak;
    contptr = savcont;
}

doswitch()
{
    register expnode *ptr;
    labstruc brk,*savbreak;
    casnode *savcase,*savlast,*cptr,*temp;
    int savdef,tests;

    getsym();
    ++swflag;

    savcase = caseptr;
    savlast = lastcase;
    savdef = deflabel;
    caseptr = NULL;

    savbreak = breakptr;
    breakptr = &brk;

    brk.labnum = deflabel = 0;
    brk.labdreg = brk.labxreg = NULL;
    brk.labsp = sp;

    need(LPAREN);
    if (ptr = optim(parsexp(0))) {
           chkdecl(ptr);
           switch(ptr->type) {
               case CHAR:
               case LONG: cvt(ptr,INT);
               case INT:
               case UNSIGN: break;
               default: integerr(ptr);
                        makedummy(ptr);
           }
           ldxexp(ptr);
           reltree(ptr);
    }
    else experr();
    need(RPAREN);

    gen(JMP,tests=getlabel(),0);
    statement();

    if (lastst == 0) gen(JMP,setlabel(&brk),0);

    label(tests);
    cptr = caseptr;
    while (cptr) {
        temp = cptr->clink;
        gen(JMPEQ,cptr->clab,cptr->cval);
        cptr->clink = freecase;
        freecase = cptr;
        cptr = temp;
    }

    clrconts();
    if (deflabel) {
        gen(JMP,deflabel,0);
        dregcont = treecopy(brk.labdreg);
        xregcont = treecopy(brk.labxreg);
    }
    uselabel(&brk);

    --swflag;
    caseptr = savcase;
    deflabel = savdef;
    lastcase = savlast;

    breakptr = savbreak;
}

docase()
{
    register casnode *ptr;
    register int val;

    getsym();
    val=constexp(0);
    need(COLON);
    clrconts();     /* clear register contents */

    if (swflag) {
        if (ptr = freecase) freecase = ptr->clink;
        else ptr = (casnode *) grab(CASESIZE);

        if (caseptr) lastcase->clink = ptr;
        else caseptr = ptr;

        lastcase = ptr;
        ptr->clink = NULL;

        ptr->cval = val;
        label(ptr->clab = getlabel());
    } else swcherr();
}

dodefault()
{
    getsym();
    if (swflag == 0) swcherr();
    if (deflabel) error("multiple defaults");
    else label(deflabel = getlabel());
    need(COLON);
    clrconts();         /* clear register contents */
}

static swcherr()
{
       error("no switch statement");
}

dodo()
{
    labstruc brk,cnt,*savbreak,*savcont;
    int looptop;

    savbreak = breakptr;
    savcont = contptr;
    breakptr = &brk;
    contptr = &cnt;

    brk.labsp = cnt.labsp = sp;
    brk.labnum = cnt.labnum = 0;
    brk.labdreg = brk.labxreg = cnt.labdreg = cnt.labxreg = NULL;
    clrconts();         /* clear register contents */
    getsym();
    label(looptop=getlabel());
    statement();

    if (sym != KEYWORD || symval != WHILE) error("while expected");
    getsym();
    uselabel(&cnt);
    cnt.labnum = looptop;
    dotest(getptest(),&cnt,&brk,FALSE);
    uselabel(&brk);

    breakptr = savbreak;
    contptr = savcont;
}

dofor()
{
    labstruc brk,cnt,tst,*savbreak,*savcont;
    int slab;
    register expnode *tptr = 0, *uptr = 0;

    savbreak = breakptr;
    savcont = contptr;
    breakptr = &brk;
    contptr = &cnt;

    brk.labsp = cnt.labsp = sp;
    brk.labnum = cnt.labnum = tst.labnum = 0;
    brk.labdreg = brk.labxreg = cnt.labdreg =
                cnt.labxreg = tst.labdreg = tst.labxreg = NULL;
    slab = getlabel();

    getsym();
    need(LPAREN);

    /* initialization */
    sidexp(0);
    need(SEMICOL);

    /* test */
    if (sym!=SEMICOL) {
        /* there IS a test */
        tptr = gettest();
        gen(JMP,setlabel(&tst),0);
    }
    need(SEMICOL);

    /* update */
    if (uptr = optim(parsexp(0))) chkdecl(uptr);
    need(RPAREN);

    /* statement */
    clrconts();         /* clear register contents */
    label(slab);
    statement();

    uselabel(&cnt);

    /* update code */
    if (uptr) emit(uptr);

    /* testcode */
    if (tptr) {
        uselabel(&tst);
        cnt.labnum = slab;
        dotest(tptr,&cnt,&brk,FALSE);
    } else gen(JMP,slab,0);

    uselabel(&brk);

    breakptr = savbreak;
    contptr = savcont;
}

doreturn()
{
    getsym();
    if (sym != SEMICOL) {
        register expnode *ptr;

        if (ptr=parsexp(0)) {
            ptr=optim(ptr);
            chkdecl(ptr);
            chkstrct(ptr);
            cvt(ptr,isptr(ftype) ? INT : ftype);
#ifdef  SPLIT
            pass2(RETURN,ptr,ftype);
#else
            switch(ftype) {
                case LONG:
                    lload(ptr);
#ifdef  DOFLOATS
                    goto common;
                case FLOAT:
                case DOUBLE:
                    dload(ptr);
common:
#endif
                    if(ptr->op!=FREG) {
                        gen(LOADIM,UREG,FREG);
                        gen(PUSH,UREG);
#ifdef  DOFLOATS
                        switch(ftype) {
                            case FLOAT:
                            case DOUBLE:
                                gen(DBLOP,MOVE,ftype);
                                break;
                            default:
                                gen(LONGOP,MOVE);
                        }
#else
                        gen(LONGOP,MOVE);
#endif
                    }
                    break;

                default:
                    lddexp(ptr);
            }
#endif
            reltree(ptr);
        }
    }

    modstk(0);
    gen(RETURN,0,0);
    lastst=RETURN;
}

dobreak()
{
    if (breakptr) modnjmp(breakptr);
    else error("break error");
    getsym();
    lastst=BREAK;
}

docont()
{
    if (contptr) modnjmp(contptr);
    else error("continue error");
    getsym();
    lastst=CONTIN;
}

modnjmp(labptr)
register labstruc *labptr;
{
    modstk(labptr->labsp);
    gen(JMP,setlabel(labptr),0);
}

dogoto()
{
    register symnode *ptr;

    getsym();

    if (sym != NAME) error("label required");
    else {
        if (ptr = checklabel()) {
            gen(GOTO,ptr->offset,0);
            ptr->x.labflg |= GONETO;
        }
        getsym();
    }
    lastst=GOTO;
}

dolabel()
{
    register symnode *ptr;

    if (ptr = checklabel()) {
        if (ptr->storage == STATIC) multidef();
        else {
            ptr->storage = STATIC;
            gen(LABEL,ptr->offset,0);
            ptr->x.labflg |= DEFINED;
        }
    }
    clrconts();         /* clear register contents */
    getsym();
    getsym();
}

static symnode *checklabel()
{
    register symnode *ptr;

    ptr = (symnode *) symval;

    if (ptr->type == LABEL) return ptr;

    if (ptr->type != UNDECL) {
        if (ptr->blklev != 0) {
            error("already a local variable");
            return NULL;
        }
        pushdown(ptr);
    }
    ptr->type = LABEL;
    ptr->storage = AUTO;
    ptr->offset = getlabel();
    ptr->x.labflg = 0;
    ptr->blklev = blklev;
    ptr->snext = labelist;
    labelist = ptr;
    return ptr;
}

static expnode *sidexp(l)
{
    register expnode *ptr;

    if (ptr = parsexp(l)) ptr = emit(optim(ptr));
    return ptr;
}

static expnode *emit(ptr)
register expnode *ptr;
{
    chkdecl(ptr);
    switch (ptr->op) {
        case INCAFT:
            ptr->op = INCBEF;
            break;
        case DECAFT:
            ptr->op = DECBEF;
            break;
    }
    ptr = tranexp(ptr);
    reltree(ptr);
    return ptr;
}

static expnode *getptest()
{
    register expnode *ptr;

    need(LPAREN);
    ptr = gettest();
    need(RPAREN);
    return ptr;
}

static expnode *gettest()
{
    register expnode *ptr;

    if (ptr = optim(parsexp(0))) chkdecl(ptr);
    else error("condition needed");
    return ptr;
}

dotest(ptr,tl,fl,nj)
register expnode *ptr;
labstruc *tl,*fl;
{
    if (ptr) {
        tranbool(ptr,tl,fl,nj);
        reltree(ptr);
    }
}

