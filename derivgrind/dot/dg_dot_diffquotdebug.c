#include "dot/dg_dot_diffquotdebug.h"

extern Bool dg_disable;

//! Next index in the buffers to be written to
static ULong dg_dot_nextindex = 0;

//! Buffer containing values of results of operations.
static ULong* dg_dot_buffer_val=NULL;
//! Buffer containing dot values of results of operations.
static ULong* dg_dot_buffer_dot=NULL;
//! Number of tape blocks fitting into the diffquotdebug buffers.
#define BUFSIZE 1000000

//! File into which dqd values are written.
static Int dg_dot_fd_val = -1;
//! File into which dqd dot values are written.
static Int dg_dot_fd_dot = -1;

void dg_dot_diffquotdebug_initialize(const HChar* path){
  // open values and dotvalues files
  ULong len = VG_(strlen)(path);
  HChar* filename = VG_(malloc)("filename in dg_dot_diffquotdebug_initialize", len+1000);
  if(!filename){
    VG_(printf)("Cannot allocate memory for filename in dg_dot_diffquotdebug_initialize.\n");
  }
  VG_(memcpy)(filename,path,len+1);

  VG_(strcpy)(filename+len, "/dg-dqd-val");
  dg_dot_fd_val = VG_(fd_open)(filename,VKI_O_WRONLY|VKI_O_CREAT|VKI_O_TRUNC|VKI_O_LARGEFILE,0777);
  if(dg_dot_fd_val==-1){
    VG_(printf)("Cannot open diffquotdebug values file at path '%s'.", filename ); tl_assert(False);
  }

  VG_(strcpy)(filename+len, "/dg-dqd-dot");
  dg_dot_fd_dot = VG_(fd_open)(filename,VKI_O_WRONLY|VKI_O_CREAT|VKI_O_TRUNC|VKI_O_LARGEFILE,0777);
  if(dg_dot_fd_dot==-1){
    VG_(printf)("Cannot open diffquotdebug dotvalues file at path '%s'.", filename ); tl_assert(False);
  }

  // allocate and zero values and dotvalues buffers
  dg_dot_buffer_val = VG_(malloc)("dqd values buffer", BUFSIZE*sizeof(ULong));
  for(ULong i=0; i<BUFSIZE; i++){
    dg_dot_buffer_val[i] = 0;
  }
  dg_dot_buffer_dot = VG_(malloc)("dqd dotvalues buffer", BUFSIZE*sizeof(ULong));
  for(ULong i=0; i<BUFSIZE; i++){
    dg_dot_buffer_dot[i] = 0;
  }
}


void dg_dot_diffquotdebug_finalize(void){
  ULong pos = (dg_dot_nextindex%BUFSIZE);
  if(pos>0){ // flush buffers
    VG_(write)(dg_dot_fd_val,dg_dot_buffer_val,pos*sizeof(ULong));
    VG_(write)(dg_dot_fd_dot,dg_dot_buffer_dot,pos*sizeof(ULong));
  }
  if(dg_dot_fd_val!=-1) VG_(close)(dg_dot_fd_val);
  if(dg_dot_fd_dot!=-1) VG_(close)(dg_dot_fd_dot);
  if(dg_dot_buffer_val) VG_(free)(dg_dot_buffer_val);
  if(dg_dot_buffer_dot) VG_(free)(dg_dot_buffer_dot);
}


static VG_REGPARM(0) void dg_add_diffquotdebug_helper(ULong value, ULong dotvalue){
  if(dg_disable==0){
    dg_dot_buffer_val[dg_dot_nextindex] = value;
    dg_dot_buffer_dot[dg_dot_nextindex] = dotvalue;
    dg_dot_nextindex++;
    if(dg_dot_nextindex%BUFSIZE==0){
      VG_(write)(dg_dot_fd_val,dg_dot_buffer_val,BUFSIZE*sizeof(ULong));
      VG_(write)(dg_dot_fd_dot,dg_dot_buffer_dot,BUFSIZE*sizeof(ULong));
    }
  }
}

void dg_add_diffquotdebug(IRSB* sb_out, IRExpr* value, IRExpr* dotvalue){
  IRType type = typeOfIRExpr(sb_out->tyenv, value);
  tl_assert(type == typeOfIRExpr(sb_out->tyenv, dotvalue));
  IRExpr *value_to_print, *dotvalue_to_print;
  switch(type){
    case Ity_F64:
      value_to_print = IRExpr_Unop(Iop_ReinterpF64asI64,value);
      dotvalue_to_print = IRExpr_Unop(Iop_ReinterpF64asI64,dotvalue);
      break;
    case Ity_F32:
      value_to_print = IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Unop(Iop_F32toF64,value));
      dotvalue_to_print = IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Unop(Iop_F32toF64,dotvalue));
      break;
    default:
      VG_(printf)("Bad type in dg_add_diffquotdebug.\n");
      return;
  }
  IRDirty* di = unsafeIRDirty_0_N(
        0,
        "dg_add_diffquotdebug_helper", VG_(fnptr_to_fnentry)(&dg_add_diffquotdebug_helper),
        mkIRExprVec_2(value_to_print, dotvalue_to_print));
  addStmtToIRSB(sb_out, IRStmt_Dirty(di));
}
