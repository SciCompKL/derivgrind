/*--------------------------------------------------------------------*/
/*--- Reverse-mode algorithmic                            dg_bar.c ---*/
/*--- differentiation using Valgrind.                              ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of DerivGrind, a tool performing forward-mode
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

#include "dg_logical.h"
#include "dg_utils.h"
#include "dg_shadow.h"



typedef void* Identifier;
typedef double Scalar;
/*! Maximal number of linear dependencies
 *  of a LinearizedExpression.
 */
#define MAX_NDEP 100
/*! Representation of a value with linear dependencies. */
typedef struct {
  Scalar value; //!< Value.
  int ndep; //!< Number of linear dependencies.
  /*! Identifies on which variables this expression depends. */
  Identifier identifier[MAX_NDEP];
  /*! Partial derivatives with respect to variables
   *  on which this expression depends.
   */
  Scalar jacobian[MAX_NDEP];
} LinExpr;

//! Storage for all linearized expressions.
LinExpr* buffer_linexpr;
//! Current number of allocated linearized expressions.
int n_linexpr=0;
//! Maximal number of linearized expressions
//! for which space has been allocated.
int max_linexpr=0;

/*! Provide space for new linearized expression.
 */
LinExpr* newLinExpr(void){
  if(n_linexpr==max_linexpr){
     LinExpr* new_buffer_linexpr = VG_(malloc)("newLinExpr",sizeof(LinExpr)*(2*n_linexpr+1));
     if(n_linexpr>0) VG_(memcpy)(new_buffer_linexpr,buffer_linexpr,sizeof(LinExpr)*n_linexpr);
     VG_(free)(buffer_linexpr);
     buffer_linexpr = new_buffer_linexpr;
     max_linexpr = 2*max_linexpr+1;
  }
  buffer_linexpr[n_linexpr].ndep = 0;
  return &buffer_linexpr[n_linexpr++];
}
/*! Discard all linearized expressions.
 */
void deleteAllLinExpr(){
  n_linexpr = 0;
}

/*! Form linear combination (k*a + l*b) of linear dependencies.
 *  The value is not set.
 *  \param[in] a - Linearized expression.
 *  \param[in] b - Linearized expression.
 *  \param[in] k - Scalar factor in front of a.
 *  \param[in] l - Scalar factor in front of b.
 */
LinExpr* linearCombinationOfDependencies(Scalar k, LinExpr* a, Scalar l, LinExpr* b ){
  LinExpr* res = newLinExpr();
  res->ndep = a->ndep + b->ndep;
  for(int i=0; i<a->ndep; i++){
    res->identifier[i] = a->identifier[i];
    res->jacobian[i] = k*a->jacobian[i];
  }
  for(int i=0; i<b->ndep; i++){
    res->identifier[a->ndep+i] = b->identifier[i];
    res->jacobian[a->ndep+i] = l*b->jacobian[i];
  }
  return res;
}

LinExpr* addLinExpr(LinExpr* a, LinExpr* b){
  LinExpr* sum = linearCombinationOfDependencies(1.,a,1.,b);
  sum->value = a->value + b->value;
  return sum;
}
LinExpr* subLinExpr(LinExpr* a, LinExpr* b){
  LinExpr* difference = linearCombinationOfDependencies(1.,a,-1.,b);
  difference->value = a->value - b->value;
  return difference;
}
LinExpr* mulLinExpr(LinExpr* a, LinExpr* b){
  LinExpr* product = linearCombinationOfDependencies(b->value,a,a->value,b);
  product->value = a->value * b->value;
  return product;
}
LinExpr* divLinExpr(LinExpr* a, LinExpr* b){
  LinExpr* quotient = linearCombinationOfDependencies(-1./b->value,a,a->value/(b->value*b->value),b);
  quotient->value = a->value / b->value;
  return quotient;
}
/*! Sort tuples (identifier,jacobian) in-place within interval [from,to).
 *  \param[in,out] a - Linearized expression.
 *  \param[in] from - First index in sorted interval.
 *  \param[in] to - First index behind sorted interval.
 *  \param[in] tmp - May use the section [from,to) of this buffer.
 */
void mergesortLinExpr(LinExpr* a, int from, int to, LinExpr* tmp){
  if(to-from<=1) return;
  int sep = (from+to)/2;
  mergesortLinExpr(a,from,sep,tmp);
  mergesortLinExpr(a,sep,to,tmp);
  int from_m = from, sep_m = sep;
  for(int i=from; i<to; i++){
    if(sep_m>=to || (from_m<sep && a->identifier[from_m] < a->identifier[sep_m]) ){
      tmp[i] = a[from_m++];
    } else {
      tmp[i] = a[sep_m++];
    }
  }
  for(int i=from; i<to; i++){
    a[i] = tmp[i];
  }
}

/*! In-place normalization of linearized expression,
 *  i.e. making identifiers occur only once.
 *  \param[in,out] a - Linearized expression, is normalized.
 *
 */
void normalizeLinExpr(LinExpr* a){
  LinExpr* ret = newLinExpr();
  mergesortLinExpr(a,0,a->ndep,ret);
  ret->ndep = 0;
  ret->value = a->value;
  for(int i=0; i<a->ndep; ){
    Identifier id = a->identifier[i];
    Scalar jacobian_sum = a->jacobian[i];
    i++;
    while(a->identifier[i]==id && i<a->ndep){
      jacobian_sum += a->jacobian[i];
      i++;
    }
    ret->identifier[ret->ndep] = id;
    ret->jacobian[ret->ndep] = jacobian_sum;
    ret->ndep++;
  }
  *a = *ret;
}
