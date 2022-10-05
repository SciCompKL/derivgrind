#include "pub_tool_basics.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_vki.h"

#include "dg_bar_tape.h"

static ULong nextindex = 1;
#define BUFSIZE 1000000
static ULong buffer[4*BUFSIZE];
static Int fd=-1;

ULong tapeAddStatement(ULong index1,ULong index2,double diff1,double diff2){
  ULong pos = (nextindex%BUFSIZE)*4;
  buffer[pos] = index1;
  buffer[pos+1] = index2;
  buffer[pos+2] = *(ULong*)&diff1;
  buffer[pos+3] = *(ULong*)&diff2;
  nextindex++;
  if(nextindex%BUFSIZE==0){
    VG_(write)(fd,buffer,4*BUFSIZE*sizeof(ULong));
  }
  return nextindex-1;
}

void dg_bar_tape_initialize(const HChar* filename){
  fd = VG_(fd_open)(filename,VKI_O_WRONLY,VKI_O_CREAT);
  if(fd==-1){
    VG_(printf)("Cannot open tape file at path '%s'.", filename );
    tl_assert(False);
  }
  for(ULong i=0; i<4*BUFSIZE; i++){
    buffer[i] = 0;
  }
}

void dg_bar_tape_finalize(void){
  ULong pos = (nextindex%BUFSIZE)*4;
  if(pos>0){ // flush buffer
    VG_(write)(fd,buffer,pos*sizeof(ULong));
  }
  VG_(close)(fd);
}
