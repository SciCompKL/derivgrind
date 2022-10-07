#ifndef DERIVGRIND_RECORDING_H
#define DERIVGRIND_RECORDING_H

#include <fcntl.h>
#include <unistd.h>

/*! \file derivgrind-recording.h
 * Helper macros for simple marking of 
 * input and output variables in the client code.
 */

static unsigned long long dg_indextmp, dg_indextmp2;
static unsigned long long const dg_zero = 0;
static double const dg_one = 1.;

/*! Mark variable as AD input and assign new 8-byte index to it.
 * 
 * You may e.g. write
 *
 *     printf("a index = %llu\n", DG_INPUT(a));
 *
 * to print the index from your client code. Or use the DG_INPUTF macro to
 * write it directly into a file.
 */
#define DG_INPUT(var) (VALGRIND_NEW_INDEX(&dg_zero,&dg_zero,&dg_zero,&dg_zero,&dg_indextmp), VALGRIND_SET_INDEX(&var,&dg_indextmp), dg_indextmp)

/*! Mark variable as AD input, assign new 8-byte index, and dump the index into a file.
 */
#define DG_INPUTF(var) \
{ \
  int fd = open("dg-inputs", O_WRONLY|O_APPEND);\
  unsigned long long index = DG_INPUT(var);\
  write(fd,&index,8);\
  close(fd);\
}

/*! Mark variable as AD output and retrieve its 8-byte index.
 * 
 * You may e.g. write
 *
 *     printf("a index = %llu\n", DG_OUTPUT(a));
 *
 * to print the index from your client code. Or use the DG_OUTPUTF macro to
 * write it directly into a file.
 */
#define DG_OUTPUT(var) (VALGRIND_GET_INDEX(&var,&dg_indextmp2), VALGRIND_NEW_INDEX(&dg_indextmp2,&dg_zero,&dg_one,&dg_zero,&dg_indextmp), dg_indextmp)

/*! Mark variable as AD output, retrieve its 8-byte index, and dump the index into a file.
 */
#define DG_OUTPUTF(var) \
{ \
  int fd = open("dg-outputs", O_WRONLY|O_APPEND);\
  unsigned long long index = DG_OUTPUT(var);\
  write(fd,&index,8);\
  close(fd);\
}

/*! Clear the files for the indices of input and output variables.
 */
#define DG_CLEARF \
{ \
  int fd = open("dg-inputs", O_WRONLY|O_CREAT|O_TRUNC,0777);\
  close(fd); \
  fd = open("dg-outputs", O_WRONLY|O_CREAT|O_TRUNC,0777);\
  close(fd); \
}



#endif
