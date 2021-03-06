#pragma once

#include "daScript/ast/ast.h"

namespace das {
    /*
     def STRUCT_NAME
        return [[STRUCT_NAME field1=init1, field2=init2, ...]]
     */
    FunctionPtr makeConstructor ( Structure * str );

    /*
     def clone(a,b:STRUCT_NAME)
        a.f1 := b.f1
        a.f2 := b.f2
        ...
    */
    FunctionPtr makeClone ( Structure * str );

    /*
        delete var
     */
    ExpressionPtr makeDelete ( const VariablePtr & var );

    /*
        a->b(args) is short for invoke(a.b, a, args)
     */
    struct ExprInvoke;
    ExprInvoke * makeInvokeMethod ( const LineInfo & at, Expression * a, const string & b );

    /*
     struct __lambda_at_line_xxx
         __lambda = @lambda_fn
        capture_field_1
        capture_field_2
     */
    StructurePtr generateLambdaStruct ( const string & lambdaName, ExprBlock * block, const set<VariablePtr> & capt );

    /*
        lambda function, i.e.
         def __lambda_function_at_line_xxx(THIS:__lambda_at_line_xxx;...block_args...)
            with THIS
                ...block_body...
     */
    FunctionPtr generateLambdaFunction ( const string & lambdaName, ExprBlock * block, const StructurePtr & ls );

    /*
         [[__lambda_at_line_xxx THIS=@__lambda_function_at_line_xxx; ba1=ba1; ba2=ba2; ... ]]
     */
    ExpressionPtr generateLambdaMakeStruct ( const StructurePtr & ls, const FunctionPtr & lf, const set<VariablePtr> & capt );

    /*
         array comprehension [[ for x in src; x_expr; where x_expr ]]
         invoke() <| $()
             let temp : Array<expr->subexpr->type>
             for .....
                 if where ....
                     push(temp, subexpr)
             return temp
    */
    struct ExprArrayComprehension;
    ExpressionPtr generateComprehension ( ExprArrayComprehension * expr );
}

