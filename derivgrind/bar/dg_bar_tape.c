/*--------------------------------------------------------------------*/
/*--- Recording-mode tape interface.                 dg_bar_tape.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Derivgrind, an automatic differentiation
   tool applicable to compiled programs.

   Copyright (C) 2022, Chair for Scientific Computing, TU Kaiserslautern
   Copyright (C) since 2023, Chair for Scientific Computing, University of Kaiserslautern-Landau
   Homepage: https://www.scicomp.uni-kl.de
   Contact: Prof. Nicolas R. Gauger (derivgrind@projects.rptu.de)

   Lead developer: Max Aehle

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
#include "pub_tool_libcbase.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_vki.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_stacktrace.h"
#include "pub_tool_libcprint.h"

#include "pub_tool_gdbserver.h"
#include "pub_tool_threadstate.h"


#include "dg_bar_tape.h"

static ULong nextindex = 1;

//! Number of tape blocks fitting into the buffer.
#define BUFSIZE 1000000

//! Buffer for tape blocks.
static ULong* buffer_tape;

//! Buffer for values.
static ULong* buffer_values;

static Int fd_tape;
static Int fd_values;
static VgFile *fp_inputs, *fp_outputs;

extern Long dg_disable;
extern Bool typegrind;
extern Bool bar_record_values;
extern Bool tape_in_ram;
extern const ULong* recording_stop_indices;

ULong tapeAddStatement(ULong index1,ULong index2,double diff1,double diff2){
  if(index1==0 && index2==0) // activity analysis
    return 0;
  else
    return tapeAddStatement_noActivityAnalysis(index1,index2,diff1,diff2);
}

ULong tapeAddStatement_noActivityAnalysis(ULong index1,ULong index2,double diff1,double diff2){
  if(dg_disable!=0) return typegrind ? 0xffffffffffffffff : 0;
  ULong pos = (nextindex%BUFSIZE);
  buffer_tape[4*pos] = index1;
  buffer_tape[4*pos+1] = index2;
  buffer_tape[4*pos+2] = *(ULong*)&diff1;
  buffer_tape[4*pos+3] = *(ULong*)&diff2;
  if(recording_stop_indices){
    Int i=0;
    ULong stop_index = recording_stop_indices[i];
    while(stop_index!=0){
      if(nextindex==stop_index){
        VG_(message)(Vg_UserMsg, "User-specified index has been reached (--record-stop).\n");
        VG_(message)(Vg_UserMsg, "Index %llu assigned at\n",nextindex);
        VG_(get_and_pp_StackTrace)(VG_(get_running_tid)(), 16);
        VG_(message)(Vg_UserMsg, "\n");
        VG_(gdbserver)(VG_(get_running_tid)());
      }
      i++;
      stop_index = recording_stop_indices[i];
    }
  }
  nextindex++;
  if(nextindex%BUFSIZE==0){
    if(tape_in_ram){
      buffer_tape = VG_(malloc)("Tape buffer reallocation.",BUFSIZE*4*sizeof(ULong));
      // The connection to previous tape buffers is lost and they will never be freed;
      // note that --tape-to-ram=yes is only for benchmarking purposes.
    } else {
      VG_(write)(fd_tape,buffer_tape,4*BUFSIZE*sizeof(ULong));
    }
  }
  if(index1==0xffffffffffffffff||index2==0xffffffffffffffff){
    VG_(message)(Vg_UserMsg, "Result of unwrapped operation used as input of differentiable operation.\n");
    VG_(message)(Vg_UserMsg, "Index of result of differentiable operation: %llu.\n",nextindex-1);
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

  VG_(strcpy)(filename+len, "/dg-tape");
  fd_tape = VG_(fd_open)(filename,VKI_O_WRONLY|VKI_O_CREAT|VKI_O_TRUNC|VKI_O_LARGEFILE,0777);
  if(fd_tape==-1){
    VG_(printf)("Cannot open tape file at path '%s'.", filename ); tl_assert(False);
  }
  if(bar_record_values){
    VG_(strcpy)(filename+len, "/dg-values");
    fd_values = VG_(fd_open)(filename,VKI_O_WRONLY|VKI_O_CREAT|VKI_O_TRUNC|VKI_O_LARGEFILE,0777);
    if(fd_values==-1){
      VG_(printf)("Cannot open values file at path '%s'.", filename ); tl_assert(False);
    }
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

  // allocate and zero buffer for tape
  buffer_tape = VG_(malloc)("Tape buffer", BUFSIZE*4*sizeof(ULong));
  for(ULong i=0; i<4*BUFSIZE; i++){
    buffer_tape[i] = 0;
  }
  // allocate and zero buffer for values
  if(bar_record_values){
    buffer_values = VG_(malloc)("Values buffer", BUFSIZE*sizeof(ULong));
    for(ULong i=0; i<BUFSIZE; i++){
      buffer_values[i] = 0;
    }
  }
}

void dg_bar_tape_write_input_index(ULong index){
  VG_(fprintf)(fp_inputs,"%llu\n", index);
}
void dg_bar_tape_write_output_index(ULong index){
  VG_(fprintf)(fp_outputs,"%llu\n", index);
}

void valuesAddStatement(double value){
  ULong pos = ((nextindex-1)%BUFSIZE);
  buffer_values[pos] = *(ULong*)&value;
  if(nextindex%BUFSIZE==0){
    VG_(write)(fd_values,buffer_values,BUFSIZE*sizeof(ULong));
  }
}

void dg_bar_tape_finalize(void){
  ULong pos = (nextindex%BUFSIZE);
  if(pos>0){ // flush buffers
    VG_(write)(fd_tape,buffer_tape,pos*4*sizeof(ULong));
    if(bar_record_values) VG_(write)(fd_values,buffer_values,pos*sizeof(ULong));
  }
  VG_(close)(fd_tape);
  VG_(close)(fd_values);
  VG_(fclose)(fp_inputs);
  VG_(fclose)(fp_outputs);

  VG_(free)(buffer_tape);
  if(bar_record_values) VG_(free)(buffer_values);
}

