#include "pub_tool_basics.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_vki.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_stacktrace.h"
#include "pub_tool_libcprint.h"

#include "dg_bar_tape.h"

static ULong nextindex = 1;
#define BUFSIZE 1000000
static ULong buffer_tape[4*BUFSIZE];
static Int fd_tape;
static VgFile *fp_inputs, *fp_outputs;

extern Long dg_disable;
extern Bool typegrind;

ULong tapeAddStatement(ULong index1,ULong index2,double diff1,double diff2){
  if(index1==0 && index2==0) // activity analysis
    return 0;
  else
    return tapeAddStatement_noActivityAnalysis(index1,index2,diff1,diff2);
}

ULong tapeAddStatement_noActivityAnalysis(ULong index1,ULong index2,double diff1,double diff2){
  if(dg_disable!=0) return typegrind ? 0xffffffffffffffff : 0;
  ULong pos = (nextindex%BUFSIZE)*4;
  buffer_tape[pos] = index1;
  buffer_tape[pos+1] = index2;
  buffer_tape[pos+2] = *(ULong*)&diff1;
  buffer_tape[pos+3] = *(ULong*)&diff2;
  nextindex++;
  if(nextindex%BUFSIZE==0){
    VG_(write)(fd_tape,buffer_tape,4*BUFSIZE*sizeof(ULong));
  }
  if(index1==0xffffffffffffffff||index2==0xffffffffffffffff){
    VG_(message)(Vg_UserMsg, "Result of unwrapped operation used as input of differentiable operation.\n");
    VG_(message)(Vg_UserMsg, "Index of result of differentiable operation: %llu.\n",nextindex);
    VG_(get_and_pp_StackTrace)(VG_(get_running_tid)(), 16);
    VG_(message)(Vg_UserMsg, "\n");
  }
  return nextindex-1;
}

void dg_bar_tape_initialize(const HChar* path){
  // open tape, input-index and output-index files
  ULong len = VG_(strlen)(path);
  HChar* filename = VG_(malloc)("filename in dg_bar_tape_initialize", len+1000);
  if(!filename){
    VG_(printf)("Cannot allocate memory for filename in dg_bar_tape_initialize.\n");
  }
  VG_(memcpy)(filename,path,len+1);

  VG_(memcpy)(filename+len, "/dg-tape", 9);
  fd_tape = VG_(fd_open)(filename,VKI_O_WRONLY|VKI_O_CREAT|VKI_O_TRUNC,0777);
  if(fd_tape==-1){
    VG_(printf)("Cannot open tape file at path '%s'.", filename ); tl_assert(False);
  }
  VG_(strcpy)(filename+len, "/dg-input-indices");
  fp_inputs = VG_(fopen)(filename,VKI_O_WRONLY|VKI_O_CREAT|VKI_O_TRUNC,0777);
  if(!fp_inputs){
    VG_(printf)("Cannot open input indices file at path '%s'.", filename ); tl_assert(False);
  }
  VG_(strcpy)(filename+len, "/dg-output-indices");
  fp_outputs = VG_(fopen)(filename,VKI_O_WRONLY|VKI_O_CREAT|VKI_O_TRUNC,0777);
  if(!fp_outputs){
    VG_(printf)("Cannot open output indices file at path '%s'.", filename ); tl_assert(False);
  }
  VG_(free)(filename);

  // zero buffer for tape
  for(ULong i=0; i<4*BUFSIZE; i++){
    buffer_tape[i] = 0;
  }
}

void dg_bar_tape_write_input_index(ULong index){
  VG_(fprintf)(fp_inputs,"%llu\n", index);
}
void dg_bar_tape_write_output_index(ULong index){
  VG_(fprintf)(fp_outputs,"%llu\n", index);
}

void dg_bar_tape_finalize(void){
  ULong pos = (nextindex%BUFSIZE)*4;
  if(pos>0){ // flush buffer
    VG_(write)(fd_tape,buffer_tape,pos*sizeof(ULong));
  }
  VG_(close)(fd_tape);
  VG_(fclose)(fp_inputs);
  VG_(fclose)(fp_outputs);
}
