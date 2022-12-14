/*--------------------------------------------------------------------*/
/*--- Derivgrind: Forward-mode algorithmic               dg_main.c ---*/
/*--- differentiation using Valgrind.                              ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Derivgrind, a tool performing forward-mode
   algorithmic differentiation of compiled programs, implemented
   in the Valgrind framework.

   Copyright (C) 2022 Chair for Scientific Computing (SciComp), TU Kaiserslautern
   Homepage: https://www.scicomp.uni-kl.de
   Contact: Prof. Nicolas R. Gauger (derivgrind@scicomp.uni-kl.de)

   Lead developer: Max Aehle (SciComp, TU Kaiserslautern)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.

   The GNU General Public License is contained in the file COPYING.
*/

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_gdbserver.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_options.h"
#include "valgrind.h"
#include "derivgrind.h"

#include "dg_utils.h"

#include "dot/dg_dot_shadow.h"
#include "bar/dg_bar_shadow.h"

#include "dot/dg_dot.h"
#include "bar/dg_bar.h"
#include "bar/dg_bar_tape.h"

/*! \page storage_convention Storage convention for shadow memory
 *
 *  To each variable x, we assign a shadow variable xd of the same size
 *  and type, with the following purpose:
 *
 *  If x is a floating-point variable (float or double), xd stores the
 *  gradient of x. If x is of another type and arose from bit-wise copying
 *  of (parts of) a floating-point variable y, xd stores a copy of the
 *  respective parts of yd. Otherwise, the value of xd is unspecified.
 *
 *  In VEX IR, there are three locations where variables can exist:
 *  - For temporary variables t_i, the shadow variables are shifted
 *    temporary variables t_(i+d1) for some offset d1.
 *  - For registers r, the shadow variables are shifted registers r+d2
 *    for some offset d2.
 *  - For memory addresses p, the shadow variables are stored by the
 *    help of a *shadow memory* library. This library allows to store
 *    another byte for each byte of "original" memory.
 */

//! Can be used to tag dg_add_print_stmt outputs.
static unsigned long stmt_counter = 0;

//! Debugging output.
Bool warn_about_unwrapped_expressions = False;

/*! Print intermediate values and dot values for difference quotient debugging.
 */
Bool diffquotdebug = False;
/*! If nonzero, do not print difference quotient debugging information.
 */
Long dg_disable = 0;

/*! Mode: d=dot/forward, b=bar/reverse/recording
 */
HChar mode = 'd';
/*! Directory for tape and index files in recording mode.
 */
const HChar* recording_directory = NULL;

/*! If true, write tape to RAM instead of file.
 *  Only for benchmarking purposes!
 */
Bool tape_in_ram = False;

static void dg_post_clo_init(void)
{
  if(typegrind && mode!='b'){
    VG_(printf)("Option --typegrind=yes can only be used in recording mode (--record=path).\n");
    tl_assert(False);
  }

  if(mode=='d'){
    dg_dot_initialize();
  } else {
    dg_bar_initialize();
    dg_bar_tape_initialize(recording_directory);
  }
}

static Bool dg_process_cmd_line_option(const HChar* arg)
{
   if VG_BOOL_CLO(arg, "--warn-unwrapped", warn_about_unwrapped_expressions) {}
   else if VG_BOOL_CLO(arg, "--diffquotdebug", diffquotdebug) {}
   else if VG_STR_CLO(arg, "--record", recording_directory) { mode = 'b'; }
   else if VG_BOOL_CLO(arg, "--typegrind", typegrind) { }
   else if VG_BOOL_CLO(arg, "--tape-in-ram", tape_in_ram) { }
   else return False;
   return True;
}

static void dg_print_usage(void)
{
   VG_(printf)(
"    --warn-unwrapped=no|yes   warn about unwrapped expressions\n"
"    --diffquotdebug=no|yes    print values and dot values of intermediate results\n"
"    --record=<directory>      switch to recording mode and store tape and indices in specified dir\n"
"    --typegrind=no|yes        record index ff...f for results of unwrapped operations\n"
   );
}

static void dg_print_debug_usage(void)
{
   VG_(printf)(
"    (none)\n"
   );
}

#include <VEX/priv/guest_generic_x87.h>
/*! React to gdb monitor commands.
 */
static
Bool dg_handle_gdb_monitor_command(ThreadId tid, HChar* req){
  HChar s[VG_(strlen)(req)+1]; //!< copy of req for strtok_r
  VG_(strcpy)(s, req);
  HChar* ssaveptr; //!< internal state of strtok_r

  const HChar commands[] = "help get set fget fset lget lset index mark"; //!< list of possible commands
  HChar* wcmd = VG_(strtok_r)(s, " ", &ssaveptr); //!< User command
  int key = VG_(keyword_id)(commands, wcmd, kwd_report_duplicated_matches);
  switch(key){
    case -2: // multiple matches
      return True;
    case -1: // not found
      return False;
    case 0: // help
      VG_(gdb_printf)(
        "monitor commands in forward mode:\n"
        "  mode <mode>       - Select which shadow map to access:\n"
        "                      dot (mode=d) or parallel (mode=p)\n"
        "  get  <addr>       - Prints shadow of binary64 (e.g. C double)\n"
        "  set  <addr> <val> - Sets shadow of binary64 (e.g. C double)\n"
        "  fget <addr>       - Prints shadow of binary32 (e.g. C float)\n"
        "  fset <addr> <val> - Sets shadow of binary32 (e.g. C float)\n"
        "  lget <addr>       - Prints shadow of x87 double extended\n"
        "  lset <addr> <val> - Sets shadow of x87 double extended\n"
        "monitor commands in recording mode:\n"
        "  index <addr>      - Prints index of variable\n"
        "  mark  <addr>      - Marks variable as input and prints its new index\n"
      );
      return True;
    case 1: case 3: case 5: { // get, fget, lget
      if(mode!='d'){ VG_(printf)("Only available in forward mode.\n"); return False; }
      HChar* address_str = VG_(strtok_r)(NULL, " ", &ssaveptr);
      HChar const* address_str_const = address_str;
      Addr address;
      if(!VG_(parse_Addr)(&address_str_const, &address)){
        VG_(gdb_printf)("Usage: get  <addr>\n"
                        "       fget <addr>\n"
                        "       lget <addr>\n");
        return False;
      }
      int size;
      switch(key){
        case 1: size = 8; break;
        case 3: size = 4; break;
        case 5: size = 10; break;
      }
      union {unsigned char l[10]; double d; float f;} shadow, init;
      dg_dot_shadowGet((void*)address, (void*)&shadow, size);
      VG_(gdb_printf)("dot value: ");
      switch(key){
        case 1:
          VG_(gdb_printf)("%.16lf\n", shadow.d);
          break;
        case 3:
          VG_(gdb_printf)("%.9f\n", shadow.f);
          break;
        case 5: {
          // convert x87 double extended to 64-bit double
          // so we can use the ordinary I/O.
          double tmp;
          convert_f80le_to_f64le(shadow.l,(unsigned char*)&tmp);
          VG_(gdb_printf)("%.16lf\n", (double)tmp);
          break;
        }
      }
      return True;
    }
    case 2: case 4: case 6: { // set, fset, lset
      if(mode!='d'){ VG_(printf)("Only available in forward mode.\n"); return False; }
      HChar* address_str = VG_(strtok_r)(NULL, " ", &ssaveptr);
      HChar const* address_str_const = address_str;
      Addr address;
      if(!VG_(parse_Addr)(&address_str_const, &address)){
        VG_(gdb_printf)("Usage: set  <addr> <shadow value>\n"
                        "       fset <addr> <shadow value>\n"
                        "       lset <addr> <shadow value>\n");
        return False;
      }
      HChar* derivative_str = VG_(strtok_r)(NULL, " ", &ssaveptr);
      union {unsigned char l[10]; double d; float f;} shadow;
      shadow.d = VG_(strtod)(derivative_str, NULL);
      int size;
      switch(key){
        case 2: size = 8; break;
        case 4: size = 4; shadow.f = (float) shadow.d; break;
        case 6: {
          // read as ordinary double and convert to x87 double extended
          // so we can use the ordinary I/O
          size = 10;
          double tmp = shadow.d;
          convert_f64le_to_f80le((unsigned char*)&tmp,shadow.l);
          break;
        }
      }
      dg_dot_shadowSet((void*)address,(void*)&shadow,size);
      return True;
    }
    case 7: case 8: { // index, mark
      if(mode!='b'){ VG_(printf)("Only available in recording mode.\n"); return False; }
      HChar* address_str = VG_(strtok_r)(NULL, " ", &ssaveptr);
      HChar const* address_str_const = address_str;
      Addr address;
      if(!VG_(parse_Addr)(&address_str_const, &address)){
        VG_(gdb_printf)("Usage: index <addr> \n");
        return False;
      }
      ULong index;
      dg_bar_shadowGet((void*)address,(void*)&index,(void*)&index+4,4);
      if(key==7){ // index
        VG_(gdb_printf)("index: %llu\n",index);
        return True;
      } else if(key==8){ // mark
        if(index!=0){
          VG_(gdb_printf)("Warning: Variable depends on other inputs, previous index was %llu.\n",index);
        }
        ULong setIndex = tapeAddStatement_noActivityAnalysis(0,0,0.,0.);
        dg_bar_shadowSet((void*)address,(void*)&setIndex,(void*)&setIndex+4,4);
        VG_(gdb_printf)("index: %llu\n",setIndex);
        return True;
      }
    }
    default:
      VG_(printf)("Error in dg_handle_gdb_monitor_command.\n");
      return False;
    }

}

/*! React to client requests like gdb monitor commands.
 */
static
Bool dg_handle_client_request(ThreadId tid, UWord* arg, UWord* ret){
  if(arg[0]==VG_USERREQ__GDB_MONITOR_COMMAND){
    Bool handled = dg_handle_gdb_monitor_command(tid, (HChar*)arg[1]);
    if(handled){
      *ret = 1;
    } else {
      *ret = 0;
    }
    return handled;
  } else if(arg[0]==VG_USERREQ__GET_DOTVALUE) {
    if(mode!='d') return True;
    void* addr = (void*) arg[1];
    void* daddr = (void*) arg[2];
    UWord size = arg[3];
    dg_dot_shadowGet((void*)addr,(void*)daddr,size);
    *ret = 1; return True;
  } else if(arg[0]==VG_USERREQ__SET_DOTVALUE) {
    if(mode!='d') return True;
    void* addr = (void*) arg[1];
    void* daddr = (void*) arg[2];
    UWord size = arg[3];
    dg_dot_shadowSet(addr,daddr,size);
    *ret = 1; return True;
  } else if(arg[0]==VG_USERREQ__DISABLE) {
    dg_disable += (Long)(arg[1]) - (Long)(arg[2]);
    *ret = 1; return True;
  } else if(arg[0]==VG_USERREQ__GET_INDEX) {
    if(mode!='b') return True;
    void* addr = (void*) arg[1];
    void* iaddr = (void*) arg[2];
    dg_bar_shadowGet((void*)addr,(void*)iaddr,(void*)iaddr+4,4);
    *ret = 1; return True;
  } else if(arg[0]==VG_USERREQ__SET_INDEX) {
    if(mode!='b') return True;
    void* addr = (void*) arg[1];
    void* iaddr = (void*) arg[2];
    dg_bar_shadowSet((void*)addr,(void*)iaddr,(void*)iaddr+4,4);
    *ret = 1; return True;
  } else if(arg[0]==VG_USERREQ__NEW_INDEX || arg[0]==VG_USERREQ__NEW_INDEX_NOACTIVITYANALYSIS) {
    if(mode!='b') return True;
    ULong* index1addr = (ULong*) arg[1];
    ULong* index2addr = (ULong*) arg[2];
    double* diff1addr = (double*) arg[3];
    double* diff2addr = (double*) arg[4];
    ULong* newindexaddr = (ULong*) arg[5];
    if(arg[0]==VG_USERREQ__NEW_INDEX)
      *newindexaddr = tapeAddStatement(*index1addr,*index2addr,*diff1addr,*diff2addr);
    else
      *newindexaddr = tapeAddStatement_noActivityAnalysis(*index1addr,*index2addr,*diff1addr,*diff2addr);
    *ret = 1; return True;
  } else if(arg[0]==VG_USERREQ__INDEX_TO_FILE){
    if(arg[1]==DG_INDEXFILE_INPUT){
      dg_bar_tape_write_input_index(*(ULong*)(arg[2]));
    } else if(arg[1]==DG_INDEXFILE_OUTPUT){
      dg_bar_tape_write_output_index(*(ULong*)(arg[2]));
    } else {
      VG_(printf)("Bad output file specification.");
      tl_assert(False);
    }
    return True;
  } else if(arg[0]==VG_USERREQ__GET_MODE){
    *ret = (UWord)mode;
    return True;
  } else {
    VG_(printf)("Unhandled user request.\n");
    return True;
  }
}

/*! Add what the original statement did, to output IRSB.
 *
 *  CAS needs special treatment: If success has already been
 *  tested in the Derivgrind instrumentation, use the result
 *  of this test.
 *  \param[in,out] sb_out - Output IRSB.
 *  \param[in] st_orig - Original statement.
 *  \param[in] diffenv - Additional data.
 */
static void dg_original_statement(DiffEnv* diffenv, IRStmt* st_orig){
  const IRStmt* st = st_orig;
  if(st->tag==Ist_CAS && diffenv->cas_succeeded != IRTemp_INVALID){
    IRCAS* det = st->Ist.CAS.details;
    IRType type = typeOfIRExpr(diffenv->sb_out->tyenv,det->expdLo);
    Bool double_element = (det->expdHi!=NULL);
    IRExpr* addr_Lo;
    IRExpr* addr_Hi;
    addressesOfCAS(det,diffenv->sb_out,&addr_Lo,&addr_Hi);
    // Set oldLo and possibly oldHi.
    addStmtToIRSB(diffenv->sb_out, IRStmt_WrTmp(det->oldLo,IRExpr_Load(det->end,type,addr_Lo)));
    if(double_element){
      addStmtToIRSB(diffenv->sb_out, IRStmt_WrTmp(det->oldHi,IRExpr_Load(det->end,type,addr_Hi)));
    }
    // Guarded write of Lo part, and possibly Hi part.
    // As Ist_StoreG causes an isel error on x86, we use an if-then-else construct.
    IRExpr* store_Lo = IRExpr_ITE(IRExpr_RdTmp(diffenv->cas_succeeded),
      det->dataLo, IRExpr_Load(det->end,type,addr_Lo));
    addStmtToIRSB(diffenv->sb_out, IRStmt_Store(det->end,addr_Lo,store_Lo));
    if(double_element){
      IRExpr* store_Hi = IRExpr_ITE(IRExpr_RdTmp(diffenv->cas_succeeded),
        det->dataHi, IRExpr_Load(det->end,type,addr_Hi));
      addStmtToIRSB(diffenv->sb_out, IRStmt_Store(det->end,addr_Hi,store_Hi));
    }
  } else { // for all other IRStmt's, just copy them
    addStmtToIRSB(diffenv->sb_out, st_orig);
  }
}

/*! \page statement_handling Instrumentation of Statements
 *
 *  In front of each "original" statement of the input IRSB, we insert
 *  an "instrumentation" statement. The precise shape of the instrumentation
 *  depends on which mode of Derivgrind is used (forward AD vs. recording),
 *  but there are some general rules:
 *  - If the original statement writes to a temporary, a register, or memory,
 *    the instrumentation writes to the respective shadow location.
 *  - The expression for the data to be written is obtained from the
 *    respective expression in the original statement, by modifications depending
 *    on the mode.
 *  - Similarly, the instrumentation for a guarded-load statement is a guarded
 *    load from shadow memory.
 *  - CAS statements form an exception: First there is a check whether the CAS is
 *    successful, and then the respective instrumented and original writes take place.
 *  - For most dirty calls, the only necessary instrumentation is an initialization of
 *    the shadow temporaries corresponding to temporaries that they might initialize.
 *    The F64<->F80 conversions need instrumentation depending on the mode.
 *
 *  For each mode, we provide an instance of the ExpressionHandling class that specifies
 *  all the details missing above, with members that point to functions for modifying
 *  expressions and accessing shadow memory.
 *
 *
 */

/*! Instrument an IRSB.
 */
static
IRSB* dg_instrument ( VgCallbackClosure* closure,
                      IRSB* sb_in,
                      const VexGuestLayout* layout, 
                      const VexGuestExtents* vge,
                      const VexArchInfo* archinfo_host,
                      IRType gWordTy, IRType hWordTy )
{
  int i;
  DiffEnv diffenv;
  IRSB* sb_out = deepCopyIRSBExceptStmts(sb_in);

  // allocate shadow temporaries and store offsets
  IRTemp nTmp =  sb_in->tyenv->types_used;
  diffenv.tmp_offset = nTmp;
  for(IRTemp t=0; t<nTmp; t++){
    newIRTemp(sb_out->tyenv, sb_in->tyenv->types[t]);
  }
  // another layer in recording mode
  if(mode=='b'){
    for(IRTemp t=0; t<nTmp; t++){
      newIRTemp(sb_out->tyenv, sb_in->tyenv->types[t]);
    }
  }

  // shadow guest state (registers)
  diffenv.gs_offset = layout->total_sizeB;

  diffenv.sb_out = sb_out;

  // copy until IMark
  i = 0;
  while (i < sb_in->stmts_used && sb_in->stmts[i]->tag != Ist_IMark) {
     addStmtToIRSB(sb_out, sb_in->stmts[i]);
     i++;
  }
  for (/* use current i*/; i < sb_in->stmts_used; i++) {
    stmt_counter++;
    IRStmt* st_orig = sb_in->stmts[i];
     //VG_(printf)("next stmt %d :",stmt_counter); ppIRStmt(st_orig); VG_(printf)("\n");

    diffenv.cas_succeeded = IRTemp_INVALID;

    if(mode=='d') dg_dot_handle_statement(&diffenv,st_orig);
    else if(mode=='b') dg_bar_handle_statement(&diffenv,st_orig);
    dg_original_statement(&diffenv,st_orig);

  }
  //VG_(printf)("from stmt %d sb :",stmt_counter); ppIRSB(sb_out); VG_(printf)("\n");

  return sb_out;
}

static void dg_fini(Int exitcode)
{
  if(mode=='d'){
    dg_dot_finalize();
  } else if(mode=='b') {
    dg_bar_finalize();
    dg_bar_tape_finalize();
  }
}

static void dg_pre_clo_init(void)
{


   VG_(details_name)            ("Derivgrind");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("a tool for forward-mode AD of compiled programs.");
   VG_(details_copyright_author)(
      "Copyright (C) 2022, and GNU GPL'd, by the \n"
      "Chair for Scientific Computing at TU Kaiserslautern.");
   VG_(details_bug_reports_to)  ("derivgrind@scicomp.uni-kl.de");

   VG_(details_avg_translation_sizeB) ( 275 );

   VG_(basic_tool_funcs)        (dg_post_clo_init,
                                 dg_instrument,
                                 dg_fini);

   VG_(needs_client_requests)     (dg_handle_client_request);

   VG_(needs_command_line_options)(dg_process_cmd_line_option,
                                   dg_print_usage,
                                   dg_print_debug_usage);

}

VG_DETERMINE_INTERFACE_VERSION(dg_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
