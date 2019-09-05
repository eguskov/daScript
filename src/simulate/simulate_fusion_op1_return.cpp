#include "daScript/misc/platform.h"

#ifdef _MSC_VER
#pragma warning(disable:4505)
#endif

#include "daScript/simulate/simulate_fusion.h"
#include "daScript/simulate/sim_policy.h"
#include "daScript/ast/ast_typedecl.h"
#include "daScript/simulate/simulate_fusion_op1.h"

namespace das {

/* Return Scalar */

#undef MATCH_ANY_OP1_NODE
#define MATCH_ANY_OP1_NODE(CTYPE,NODENAME,COMPUTE) \
    else if ( is(info,tnode->x,NODENAME,(typeName<CTYPE>::name())) ) { result = context->code->makeNode<SimNode_Op1##COMPUTE>(); }

#undef IMPLEMENT_ANY_OP1_NODE
#define IMPLEMENT_ANY_OP1_NODE(INLINE,OPNAME,TYPE,CTYPE,COMPUTE) \
    struct SimNode_Op1##COMPUTE : SimNode_Op1Fusion { \
        virtual vec4f eval ( Context & context ) override { \
            auto lv =  FUSION_OP_PTR_VALUE(CTYPE,subexpr.compute##COMPUTE(context)); \
            context.stopFlags |= EvalFlags::stopForReturn; \
            context.abiResult() = cast<CTYPE>::from(lv); \
            return v_zero(); \
        } \
    };

#include "daScript/simulate/simulate_fusion_op1_impl.h"
#include "daScript/simulate/simulate_fusion_op1_perm.h"

    IMPLEMENT_OP1_SCALAR_FUSION_POINT(Return);

/* Return Vec */

#undef IMPLEMENT_ANY_OP1_NODE
#define IMPLEMENT_ANY_OP1_NODE(INLINE,OPNAME,TYPE,CTYPE,COMPUTE) \
    struct SimNode_Op1##COMPUTE : SimNode_Op1Fusion { \
        virtual vec4f eval ( Context & context ) override { \
            auto lv =  subexpr.compute##COMPUTE(context); \
            context.stopFlags |= EvalFlags::stopForReturn; \
            context.abiResult() = v_ldu((const float *) lv); \
            return v_zero(); \
        } \
    };

#include "daScript/simulate/simulate_fusion_op1_impl.h"
#include "daScript/simulate/simulate_fusion_op1_perm.h"

    IMPLEMENT_OP1_NUMERIC_VEC(Return);

#undef REGISTER_OP1_FUSION_POINT
#define REGISTER_OP1_FUSION_POINT(OPNAME,TYPE,CTYPE) \
    (*g_fusionEngine)[#OPNAME].push_back(make_shared<Op1FusionPoint_##OPNAME##_##CTYPE>());

#include "daScript/simulate/simulate_fusion_op1_reg.h"

    void createFusionEngine_op1()
    {
        REGISTER_OP1_SCALAR_FUSION_POINT(Return);
        REGISTER_OP1_NUMERIC_VEC(Return);
    }
}