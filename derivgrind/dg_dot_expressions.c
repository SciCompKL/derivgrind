#include "dg_dot_expressions.h"

#include "pub_tool_basics.h"
#include "pub_tool_libcassert.h"
#include "dg_logical.h"
#include "dg_utils.h"

#include "dg_shadow.h"

extern void* sm_dot;

IRExpr* differentiate_expr(IRExpr const* ex, DiffEnv* diffenv ){
  if(ex->tag==Iex_Qop){
    IRQop* rex = ex->Iex.Qop.details;
    IRExpr* arg1 = rex->arg1;
    IRExpr* arg2 = rex->arg2;
    IRExpr* arg3 = rex->arg3;
    IRExpr* arg4 = rex->arg4;
    IRExpr* d2 = differentiate_expr(arg2,diffenv);
    IRExpr* d3 = differentiate_expr(arg3,diffenv);
    IRExpr* d4 = differentiate_expr(arg4,diffenv);
    if(d2==NULL || d3==NULL || d4==NULL) return NULL;
    switch(rex->op){
      /*! Define derivative of e.g. Iop_MSubF32.
       * \param addsub - Add or Sub
       * \param suffix - F32 or F64
       */
      #define DERIVATIVE_OF_MADDSUB_QOP(addsub, suffix) \
      case Iop_M##addsub##suffix: {\
        IRType originaltype; \
        IRExpr* arg2_f64 = convertToF64(arg2,diffenv,&originaltype); \
        IRExpr* arg3_f64 = convertToF64(arg3,diffenv,&originaltype); \
        IRExpr* d2_f64 = convertToF64(d2,diffenv,&originaltype); \
        IRExpr* d3_f64 = convertToF64(d3,diffenv,&originaltype); \
        IRExpr* d4_f64 = convertToF64(d4,diffenv,&originaltype); \
        IRExpr* res = IRExpr_Triop(Iop_##addsub##F64,arg1, \
          IRExpr_Triop(Iop_MulF64,arg1,d2_f64,arg3_f64), \
          IRExpr_Qop(Iop_M##addsub##F64,arg1,arg2_f64,d3_f64,d4_f64) \
         ); \
        return convertFromF64(res, originaltype); \
      }
      DERIVATIVE_OF_MADDSUB_QOP(Add, F64)
      DERIVATIVE_OF_MADDSUB_QOP(Sub, F64)
      DERIVATIVE_OF_MADDSUB_QOP(Add, F32)
      DERIVATIVE_OF_MADDSUB_QOP(Sub, F32)
      case Iop_64x4toV256: {
        IRExpr* d1 = differentiate_expr(arg2,diffenv);
        if(d1)
          return IRExpr_Qop(rex->op, d1,d2,d3,d4);
        else
          return NULL;
      }

      default:
        return NULL;
    }
  } else if(ex->tag==Iex_Triop){
    IRTriop* rex = ex->Iex.Triop.details;
    IRExpr* arg1 = rex->arg1;
    IRExpr* arg2 = rex->arg2;
    IRExpr* arg3 = rex->arg3;
    IRExpr* d2 = differentiate_expr(arg2,diffenv);
    IRExpr* d3 = differentiate_expr(arg3,diffenv);
    if(d2==NULL || d3==NULL) return NULL;
    /*! Define derivative for addition IROp.
     *  \param[in] suffix - Specifies addition IROp, e.g. F64 gives Iop_AddF64.
     */
    #define DERIVATIVE_OF_TRIOP_ADD(suffix) \
      case Iop_Add##suffix: \
        return IRExpr_Triop(Iop_Add##suffix, arg1, d2, d3);
    /*! Define derivative for subtraction IROp.
     */
    #define DERIVATIVE_OF_TRIOP_SUB(suffix) \
      case Iop_Sub##suffix: \
        return IRExpr_Triop(Iop_Sub##suffix, arg1, d2, d3);
    /*! Define derivative for multiplication IROp.
     */
    #define DERIVATIVE_OF_TRIOP_MUL(suffix) \
      case Iop_Mul##suffix: \
        return IRExpr_Triop(Iop_Add##suffix,arg1, \
          IRExpr_Triop(Iop_Mul##suffix, arg1, d2,arg3), \
          IRExpr_Triop(Iop_Mul##suffix, arg1, d3,arg2) \
        );
    /*! Define derivative for division IROp.
     */
    #define DERIVATIVE_OF_TRIOP_DIV(suffix) \
      case Iop_Div##suffix: \
        return IRExpr_Triop(Iop_Div##suffix,arg1, \
          IRExpr_Triop(Iop_Sub##suffix, arg1, \
            IRExpr_Triop(Iop_Mul##suffix, arg1, d2,arg3), \
            IRExpr_Triop(Iop_Mul##suffix, arg1, d3,arg2) \
          ), \
          IRExpr_Triop(Iop_Mul##suffix, arg1, arg3, arg3) \
        );
    /*! Define derivatives for four basic arithmetic operations.
     */
    #define DERIVATIVE_OF_BASICOP_ALL(suffix) \
      DERIVATIVE_OF_TRIOP_ADD(suffix) \
      DERIVATIVE_OF_TRIOP_SUB(suffix) \
      DERIVATIVE_OF_TRIOP_MUL(suffix) \
      DERIVATIVE_OF_TRIOP_DIV(suffix) \

    switch(rex->op){
      DERIVATIVE_OF_BASICOP_ALL(F64) // e.g. Iop_AddF64
      DERIVATIVE_OF_BASICOP_ALL(F32) // e.g. Iop_AddF32
      DERIVATIVE_OF_BASICOP_ALL(64Fx2) // e.g. Iop_Add64Fx2
      DERIVATIVE_OF_BASICOP_ALL(64Fx4) // e.g. Iop_Add64Fx4
      DERIVATIVE_OF_BASICOP_ALL(32Fx4) // e.g. Iop_Add32Fx4
      DERIVATIVE_OF_BASICOP_ALL(32Fx8) // e.g. Iop_Add32Fx8
      // there is no Iop_Div32Fx2
      DERIVATIVE_OF_TRIOP_ADD(32Fx2) DERIVATIVE_OF_TRIOP_SUB(32Fx2) DERIVATIVE_OF_TRIOP_MUL(32Fx2)
      case Iop_AtanF64: {
        IRExpr* fraction = IRExpr_Triop(Iop_DivF64,arg1,arg2,arg3);
        IRExpr* fraction_d = IRExpr_Triop(Iop_DivF64,arg1,
          IRExpr_Triop(Iop_SubF64, arg1,
            IRExpr_Triop(Iop_MulF64, arg1, d2,arg3),
            IRExpr_Triop(Iop_MulF64, arg1, d3,arg2)
          ),
          IRExpr_Triop(Iop_MulF64, arg1, arg3, arg3)
        );
        return IRExpr_Triop(Iop_DivF64,arg1,
          fraction_d,
          IRExpr_Triop(Iop_AddF64,arg1,
            IRExpr_Const(IRConst_F64(1.)),
            IRExpr_Triop(Iop_MulF64,arg1,fraction,fraction)
          )
        );
      }
      case Iop_ScaleF64:
        return IRExpr_Triop(Iop_ScaleF64,arg1,d2,arg3);
      case Iop_Yl2xF64:
        return IRExpr_Triop(Iop_AddF64,arg1,
          IRExpr_Triop(Iop_Yl2xF64,arg1,d2,arg3),
          IRExpr_Triop(Iop_DivF64,arg1,
            IRExpr_Triop(Iop_MulF64,arg1,arg2,d3),
            IRExpr_Triop(Iop_MulF64,arg1,
              IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)),
              arg3))
        );
      case Iop_Yl2xp1F64:
        return IRExpr_Triop(Iop_AddF64,arg1,
          IRExpr_Triop(Iop_Yl2xp1F64,arg1,d2,arg3),
          IRExpr_Triop(Iop_DivF64,arg1,
            IRExpr_Triop(Iop_MulF64,arg1,arg2,d3),
            IRExpr_Triop(Iop_MulF64,arg1,
              IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)),
              IRExpr_Triop(Iop_AddF64, arg1, arg3, IRExpr_Const(IRConst_F64(1.)))))
        );

      default:
        return NULL;
    }
  } else if(ex->tag==Iex_Binop) {
    IROp op = ex->Iex.Binop.op;
    IRExpr* arg1 = ex->Iex.Binop.arg1;
    IRExpr* arg2 = ex->Iex.Binop.arg2;
    IRExpr* d2 = differentiate_expr(arg2,diffenv);
    if(d2==NULL) return NULL;
    switch(op){
      /*! Handle logical "and", "or", "xor" on
       *  32, 64, 128 and 256 bit.
       *  \param[in] Op - And, Or or Xor
       *  \param[in] op - and, or or xor
       */
      #define DG_HANDLE_LOGICAL(Op, op) \
      case Iop_##Op##32: { \
        IRExpr* d1 = differentiate_expr(arg1,diffenv); \
        IRExpr* zero32 = IRExpr_Const(IRConst_U32(0)); \
        IRExpr* arg1_64 = IRExpr_Binop(Iop_32HLto64, zero32, arg1); \
        IRExpr* d1_64 = IRExpr_Binop(Iop_32HLto64, zero32, d1); \
        IRExpr* arg2_64 = IRExpr_Binop(Iop_32HLto64, zero32, arg2); \
        IRExpr* d2_64 = IRExpr_Binop(Iop_32HLto64, zero32, d2); \
        IRExpr* res = mkIRExprCCall(Ity_I64, \
          0, \
          "dg_logical_" #op "64", &dg_logical_##op##64, \
          mkIRExprVec_4(arg1_64, d1_64, arg2_64, d2_64) \
        ); \
        return IRExpr_Unop(Iop_64to32, res); \
      } \
      case Iop_##Op##64: { \
        IRExpr* d1 = differentiate_expr(arg1,diffenv); \
        IRExpr* res = mkIRExprCCall(Ity_I64, \
          0, \
          "dg_logical_" #op "64", &dg_logical_##op##64, \
          mkIRExprVec_4(arg1, d1, arg2, d2) \
        ); \
        return res; \
      } \
      case Iop_##Op##V128: { \
        IRExpr* d1 = differentiate_expr(arg1,diffenv); \
        IRExpr *res[2]; \
        for(int i=0; i<2; i++){ /* 0=low half, 1=high half */ \
          IROp selector = i==0? Iop_V128to64 : Iop_V128HIto64; \
          IRExpr* arg1_part = IRExpr_Unop(selector, arg1); \
          IRExpr* arg2_part = IRExpr_Unop(selector, arg2); \
          IRExpr* d1_part = IRExpr_Unop(selector, d1); \
          IRExpr* d2_part = IRExpr_Unop(selector, d2); \
          res[i] = mkIRExprCCall(Ity_I64, \
                0, \
                "dg_logical_" #op "64", &dg_logical_##op##64, \
                mkIRExprVec_4(arg1_part, d1_part, arg2_part, d2_part) \
             ); \
        } \
        return IRExpr_Binop(Iop_64HLtoV128, res[1], res[0]); \
      } \
      case Iop_##Op##V256: { \
        IRExpr* d1 = differentiate_expr(arg1,diffenv); \
        IRExpr *res[4]; \
        for(int i=0; i<4; i++){ /* 0=low quarter, ..., 3=high quarter */ \
          IROp selector; \
          switch(i){ \
            case 0: selector = Iop_V256to64_0; break; \
            case 1: selector = Iop_V256to64_1; break; \
            case 2: selector = Iop_V256to64_2; break; \
            case 3: selector = Iop_V256to64_3; break; \
          } \
          IRExpr* arg1_part = IRExpr_Unop(selector, arg1); \
          IRExpr* arg2_part = IRExpr_Unop(selector, arg2); \
          IRExpr* d1_part = IRExpr_Unop(selector, d1); \
          IRExpr* d2_part = IRExpr_Unop(selector, d2); \
          res[i] = mkIRExprCCall(Ity_I64, \
                0, \
                "dg_logical_" #op "64", &dg_logical_##op##64, \
                mkIRExprVec_4(arg1_part, d1_part, arg2_part, d2_part) \
             ); \
        } \
        return IRExpr_Qop(Iop_64x4toV256, res[3], res[2], res[1], res[0]); \
      }
      DG_HANDLE_LOGICAL(And, and)
      DG_HANDLE_LOGICAL(Or, or)
      DG_HANDLE_LOGICAL(Xor, xor)

      /*! Define derivative for square root IROp.
       */
      #define DERIVATIVE_OF_BINOP_SQRT(suffix, consttwo) \
        case Iop_Sqrt##suffix: { \
          IRExpr* numerator = d2; \
          IRExpr* denominator =  IRExpr_Triop(Iop_Mul##suffix, arg1, consttwo, IRExpr_Binop(Iop_Sqrt##suffix, arg1, arg2) ); \
          return IRExpr_Triop(Iop_Div##suffix, arg1, numerator, denominator); \
        }
      DERIVATIVE_OF_BINOP_SQRT(F64, IRExpr_Const(IRConst_F64(2.)))
      DERIVATIVE_OF_BINOP_SQRT(F32, IRExpr_Const(IRConst_F32(2.)))
      DERIVATIVE_OF_BINOP_SQRT(64Fx2, IRExpr_Binop(Iop_64HLtoV128,  \
        IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Const(IRConst_F64(2.))), \
        IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Const(IRConst_F64(2.))) ))
      DERIVATIVE_OF_BINOP_SQRT(64Fx4, IRExpr_Qop(Iop_64x4toV256,  \
        IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Const(IRConst_F64(2.))), \
        IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Const(IRConst_F64(2.))), \
        IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Const(IRConst_F64(2.))), \
        IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Const(IRConst_F64(2.))) ))
      DERIVATIVE_OF_BINOP_SQRT(32Fx4, IRExpr_Binop(Iop_64HLtoV128,  \
        IRExpr_Binop(Iop_32HLto64, \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))), \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))) ), \
        IRExpr_Binop(Iop_32HLto64,
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))), \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))) ) ))
      DERIVATIVE_OF_BINOP_SQRT(32Fx8, IRExpr_Qop(Iop_64x4toV256,  \
        IRExpr_Binop(Iop_32HLto64, \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))), \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))) ), \
        IRExpr_Binop(Iop_32HLto64, \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))), \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))) ), \
        IRExpr_Binop(Iop_32HLto64, \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))), \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))) ), \
        IRExpr_Binop(Iop_32HLto64,
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))), \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))) ) ))


      case Iop_F64toF32: {
        return IRExpr_Binop(Iop_F64toF32, arg1, d2);
      }
      case Iop_2xm1F64:
        return IRExpr_Triop(Iop_MulF64, arg1,
          IRExpr_Triop(Iop_MulF64, arg1,
            IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)),
            d2),
          IRExpr_Triop(Iop_AddF64, arg1,
            IRExpr_Const(IRConst_F64(1.)),
            IRExpr_Binop(Iop_2xm1F64,arg1,arg2)
        ));
      case Iop_Mul64F0x2: {
        IRExpr* d1 = differentiate_expr(arg1,diffenv);
        if(!d1) return NULL;
        return IRExpr_Binop(Iop_Add64F0x2,
          IRExpr_Binop(Iop_Mul64F0x2,d1,arg2), // the order is important here
          IRExpr_Binop(Iop_Mul64F0x2,arg1,d2)
        );
      }
      case Iop_Mul32F0x4: {
        IRExpr* d1 = differentiate_expr(arg1,diffenv);
        if(!d1) return NULL;
        return IRExpr_Binop(Iop_Add32F0x4,
          IRExpr_Binop(Iop_Mul32F0x4,d1,arg2), // the order is important here
          IRExpr_Binop(Iop_Mul32F0x4,arg1,d2)
        );
      }
      case Iop_Div64F0x2: {
        IRExpr* d1 = differentiate_expr(arg1,diffenv);
        if(!d1) return NULL;
        return IRExpr_Binop(Iop_Div64F0x2,
          IRExpr_Binop(Iop_Sub64F0x2,
            IRExpr_Binop(Iop_Mul64F0x2,d1,arg2), // the order is important here
            IRExpr_Binop(Iop_Mul64F0x2,arg1,d2)
          ),
          IRExpr_Binop(Iop_Mul64F0x2,arg2,arg2)
        );
      }
      case Iop_Div32F0x4: {
        IRExpr* d1 = differentiate_expr(arg1,diffenv);
        if(!d1) return NULL;
        return IRExpr_Binop(Iop_Div32F0x4,
          IRExpr_Binop(Iop_Sub32F0x4,
            IRExpr_Binop(Iop_Mul32F0x4,d1,arg2), // the order is important here
            IRExpr_Binop(Iop_Mul32F0x4,arg1,d2)
          ),
          IRExpr_Binop(Iop_Mul32F0x4,arg2,arg2)
        );
      }
      case Iop_Min64F0x2: {
        IRExpr* d1 = differentiate_expr(arg1,diffenv);
        if(!d1) return NULL;
        IRExpr* d1_lo = IRExpr_Unop(Iop_V128to64, d1);
        IRExpr* d2_lo = IRExpr_Unop(Iop_V128to64, d2);
        IRExpr* d1_hi = IRExpr_Unop(Iop_V128HIto64, d1);
        IRExpr* arg1_lo_f = IRExpr_Unop(Iop_ReinterpI64asF64, IRExpr_Unop(Iop_V128to64, arg1));
        IRExpr* arg2_lo_f = IRExpr_Unop(Iop_ReinterpI64asF64, IRExpr_Unop(Iop_V128to64, arg2));
        IRExpr* cond = IRExpr_Binop(Iop_CmpF64, arg1_lo_f, arg2_lo_f);
        return IRExpr_Binop(Iop_64HLtoV128,
          d1_hi,
          IRExpr_ITE(IRExpr_Unop(Iop_32to1,cond),d1_lo,d2_lo)
        );
      }
      // the following operations produce an F64 zero derivative
      case Iop_I64StoF64:
      case Iop_I64UtoF64:
      case Iop_RoundF64toInt:
        return mkIRConst_zero(Ity_F64);
      // the following operations produce an F32 zero derivative
      case Iop_I64StoF32:
      case Iop_I64UtoF32:
      case Iop_I32StoF32:
      case Iop_I32UtoF32:
        return mkIRConst_zero(Ity_F32);
      // the following operations only "transport", so they are applied
      // on the derivatives in the same way as for primal values
      case Iop_64HLto128: case Iop_32HLto64:
      case Iop_16HLto32: case Iop_8HLto16:
      case Iop_64HLtoV128: case Iop_V128HLtoV256:
      case Iop_Add64F0x2: case Iop_Sub64F0x2:
      case Iop_Add32F0x4: case Iop_Sub32F0x4:
      case Iop_SetV128lo32: case Iop_SetV128lo64:
      case Iop_InterleaveHI8x16: case Iop_InterleaveHI16x8:
      case Iop_InterleaveHI32x4: case Iop_InterleaveHI64x2:
      case Iop_InterleaveLO8x16: case Iop_InterleaveLO16x8:
      case Iop_InterleaveLO32x4: case Iop_InterleaveLO64x2:
        {
          IRExpr* d1 = differentiate_expr(arg1,diffenv);
          if(!d1) return NULL;
          else return IRExpr_Binop(op, d1,d2);
        }
      default:
        return NULL;
    }
  } else if(ex->tag==Iex_Unop) {
    IROp op = ex->Iex.Unop.op;
    IRExpr* arg = ex->Iex.Unop.arg;
    IRExpr* d = differentiate_expr(arg,diffenv);
    if(d==NULL) return NULL;
    switch(op){
      case Iop_AbsF64: {
        // If arg >= 0, we get Ircr_GT or Ircr_EQ, thus the Iop_32to1 gives a 0 bit.
        IRExpr* cond = IRExpr_Binop(Iop_CmpF64, arg, IRExpr_Const(IRConst_F64(0.)));
        IRExpr* minus_d = IRExpr_Unop(Iop_NegF64, d);
        return IRExpr_ITE(IRExpr_Unop(Iop_32to1,cond), minus_d, d);
      }
      case Iop_AbsF32: {
        IRExpr* cond = IRExpr_Binop(Iop_CmpF32, arg, IRExpr_Const(IRConst_F32(0.)));
        IRExpr* minus_d = IRExpr_Unop(Iop_NegF32, d);
        return IRExpr_ITE(IRExpr_Unop(Iop_32to1,cond), minus_d, d);
      }

      /*! Define derivative for square root IROp.
       */
      #define DERIVATIVE_OF_UNOP_SQRT(suffix,consttwo) \
        case Iop_Sqrt##suffix: { \
          IRExpr* numerator = d; \
          IRExpr* consttwo_32 = IRExpr_Unop(Iop_ReinterpF32asI32, IRExpr_Const(IRConst_F32(2.))); \
          IRExpr* consttwo_32x2 = IRExpr_Binop(Iop_32HLto64, consttwo_32,consttwo_32); \
          IRExpr* consttwo_64 = IRExpr_Unop(Iop_ReinterpF64asI64, IRExpr_Const(IRConst_F64(2.))); \
          IRExpr* default_rounding = IRExpr_Const(IRConst_U32(Irrm_ZERO)); \
          IRExpr* denominator =  IRExpr_Triop(Iop_Mul##suffix, default_rounding, consttwo, IRExpr_Unop(Iop_Sqrt##suffix, arg) ); \
          return IRExpr_Triop(Iop_Div##suffix, default_rounding, numerator, denominator); \
        }
      DERIVATIVE_OF_UNOP_SQRT(64Fx2, IRExpr_Binop(Iop_64HLtoV128,consttwo_64,consttwo_64))
      DERIVATIVE_OF_UNOP_SQRT(64Fx4, IRExpr_Qop(Iop_64x4toV256,consttwo_64,consttwo_64,consttwo_64,consttwo_64))
      DERIVATIVE_OF_UNOP_SQRT(32Fx4, IRExpr_Binop(Iop_64HLtoV128, consttwo_32x2,consttwo_32x2))
      DERIVATIVE_OF_UNOP_SQRT(32Fx8, IRExpr_Qop(Iop_64x4toV256,consttwo_32x2,consttwo_32x2,consttwo_32x2,consttwo_32x2))

      case Iop_Sqrt64F0x2: {
        IRExpr* numerator = d;
        IRExpr* consttwo_f64 = IRExpr_Const(IRConst_F64(2.0));
        IRExpr* consttwo_i64 = IRExpr_Unop(Iop_ReinterpF64asI64, consttwo_f64);
        IRExpr* consttwo_v128 = IRExpr_Binop(Iop_64HLtoV128, consttwo_i64, consttwo_i64);
        IRExpr* denominator =  IRExpr_Binop(Iop_Mul64F0x2, consttwo_v128, IRExpr_Unop(Iop_Sqrt64F0x2, arg) );
        return IRExpr_Binop(Iop_Div64F0x2, numerator, denominator);
        // fortunately, this is also right on the upper half of the V128
      }
      case Iop_Sqrt32F0x4: {
        IRExpr* numerator = d;
        IRExpr* consttwo_f32 = IRExpr_Const(IRConst_F32(2.0));
        IRExpr* consttwo_i32 = IRExpr_Unop(Iop_ReinterpF32asI32, consttwo_f32);
        IRExpr* consttwo_i64 = IRExpr_Binop(Iop_32HLto64, consttwo_i32, consttwo_i32);
        IRExpr* consttwo_v128 = IRExpr_Binop(Iop_64HLtoV128, consttwo_i64, consttwo_i64);
        IRExpr* denominator =  IRExpr_Binop(Iop_Mul32F0x4, consttwo_v128, IRExpr_Unop(Iop_Sqrt32F0x4, arg) );
        return IRExpr_Binop(Iop_Div32F0x4, numerator, denominator);
        // fortunately, this is also right on the upper 3/4 of the V128
      }
      case Iop_I32StoF64:
      case Iop_I32UtoF64:
        return IRExpr_Const(IRConst_F64(0.));
      // the following instructions are simply applied to the derivative as well
      case Iop_F32toF64:
      case Iop_ReinterpI64asF64: case Iop_ReinterpF64asI64:
      case Iop_ReinterpI32asF32: case Iop_ReinterpF32asI32:
      case Iop_NegF64: case Iop_NegF32:
      case Iop_64to8: case Iop_32to8: case Iop_64to16:
      case Iop_16to8: case Iop_16HIto8:
      case Iop_32to16: case Iop_32HIto16:
      case Iop_64to32: case Iop_64HIto32:
      case Iop_V128to64: case Iop_V128HIto64:
      case Iop_V256toV128_0: case Iop_V256toV128_1:
      case Iop_8Uto16: case Iop_8Uto32: case Iop_8Uto64:
      case Iop_16Uto32: case Iop_16Uto64:
      case Iop_32Uto64:
      case Iop_8Sto16: case Iop_8Sto32: case Iop_8Sto64:
      case Iop_16Sto32: case Iop_16Sto64:
      case Iop_32Sto64:
      case Iop_ZeroHI64ofV128: case Iop_ZeroHI96ofV128:
      case Iop_ZeroHI112ofV128: case Iop_ZeroHI120ofV128:
      case Iop_64UtoV128: case Iop_32UtoV128:
      case Iop_V128to32:
      case Iop_128HIto64:
      case Iop_128to64:
        return IRExpr_Unop(op, d);

      default:
        return NULL;
    }
  } else if(ex->tag==Iex_Const) {
    IRConstTag type = ex->Iex.Const.con->tag;
    switch(type){
      case Ico_F64: return IRExpr_Const(IRConst_F64(0.));
      case Ico_F64i: return IRExpr_Const(IRConst_F64i(0));    // TODO why are these not Ity_F64 etc. types?
      case Ico_F32: return IRExpr_Const(IRConst_F32(0.));
      case Ico_F32i: return IRExpr_Const(IRConst_F32i(0));
      case Ico_U1: return IRExpr_Const(IRConst_U1(0));
      case Ico_U8: return IRExpr_Const(IRConst_U8(0));
      case Ico_U16: return IRExpr_Const(IRConst_U16(0));
      case Ico_U32: return IRExpr_Const(IRConst_U32(0));
      case Ico_U64: return IRExpr_Const(IRConst_U64(0));
      case Ico_U128: return IRExpr_Const(IRConst_U128(0));
      case Ico_V128: return IRExpr_Const(IRConst_V128(0));
      case Ico_V256: return IRExpr_Const(IRConst_V256(0));
      default: tl_assert(False); return NULL;
    }
  } else if(ex->tag==Iex_ITE) {
    IRExpr* dtrue = differentiate_expr(ex->Iex.ITE.iftrue, diffenv);
    IRExpr* dfalse = differentiate_expr(ex->Iex.ITE.iffalse, diffenv);
    if(dtrue==NULL || dfalse==NULL) return NULL;
    else return IRExpr_ITE(ex->Iex.ITE.cond, dtrue, dfalse);
  } else if(ex->tag==Iex_RdTmp) {
    return IRExpr_RdTmp( ex->Iex.RdTmp.tmp + diffenv->tmp_offset[1] );
  } else if(ex->tag==Iex_Get) {
    return IRExpr_Get(ex->Iex.Get.offset+diffenv->gs_offset[1],ex->Iex.Get.ty);
  } else if(ex->tag==Iex_GetI) {
    IRRegArray* descr = ex->Iex.GetI.descr;
    IRExpr* ix = ex->Iex.GetI.ix;
    Int bias = ex->Iex.GetI.bias;
    IRRegArray* descr_diff = mkIRRegArray(descr->base+diffenv->gs_offset[1],descr->elemTy,descr->nElems);
    return IRExpr_GetI(descr_diff,ix,bias+diffenv->gs_offset[1]);
  } else if (ex->tag==Iex_Load) {
    return loadShadowMemory(sm_dot,diffenv->sb_out,ex->Iex.Load.addr,ex->Iex.Load.ty);
  } else {
    return NULL;
  }
}


IRExpr* differentiate_or_zero(IRExpr* expr, DiffEnv* diffenv, Bool warn, const char* operation){
  IRExpr* diff = differentiate_expr(expr,diffenv);
  if(diff){
    return diff;
  } else {
    if(warn){
      VG_(printf)("Warning: Expression\n");
      ppIRExpr(expr);
      VG_(printf)("\ncould not be differentiated, %s'ing zero instead.\n\n", operation);
    }
    return mkIRConst_zero(typeOfIRExpr(diffenv->sb_out->tyenv,expr));
  }
}
