#include "daScript/misc/platform.h"

#include "daScript/simulate/simulate.h"
#include "daScript/simulate/simulate_nodes.h"
#include "daScript/simulate/runtime_string.h"
#include "daScript/simulate/debug_print.h"
#include <stdarg.h>

// this is here for the default implementation of to_out and to_err
#include <setjmp.h>

extern void os_debug_break();

namespace das
{
    // this is here to occasionally investigate untyped evaluation paths
    #define WARN_SLOW_CAST(TYPE)
    // #define WARN_SLOW_CAST(TYPE)    DAS_ASSERTF(0, "internal perofrmance issue, casting eval to eval##TYPE" );

    SimNode * SimNode::copyNode ( Context &, NodeAllocator * code ) {
        auto prefix = ((NodePrefix *)this) - 1;
        DAS_ASSERTF(prefix->magic==0xdeadc0de,"node was allocated on the heap without prefix");
        char * newNode = code->allocate(prefix->size);
        memcpy ( newNode, (char *)this, prefix->size );
        return (SimNode *) newNode;
    }

    bool SimNode::evalBool ( Context & context ) {
        WARN_SLOW_CAST(Bool);
        return cast<bool>::to(eval(context));
    }

    float SimNode::evalFloat ( Context & context ) {
        WARN_SLOW_CAST(Float);
        return cast<float>::to(eval(context));
    }

    double SimNode::evalDouble(Context & context) {
        WARN_SLOW_CAST(Double);
        return cast<double>::to(eval(context));
    }

    int32_t SimNode::evalInt ( Context & context ) {
        WARN_SLOW_CAST(Int);
        return cast<int32_t>::to(eval(context));
    }

    uint32_t SimNode::evalUInt ( Context & context ) {
        WARN_SLOW_CAST(UInt);
        return cast<uint32_t>::to(eval(context));
    }

    int64_t SimNode::evalInt64 ( Context & context ) {
        WARN_SLOW_CAST(Int64);
        return cast<int64_t>::to(eval(context));
    }

    uint64_t SimNode::evalUInt64 ( Context & context ) {
        WARN_SLOW_CAST(UInt64);
        return cast<uint64_t>::to(eval(context));
    }

    char * SimNode::evalPtr ( Context & context ) {
        WARN_SLOW_CAST(Ptr);
        return cast<char *>::to(eval(context));
    }

    vec4f SimNode_DeleteStructPtr::eval ( Context & context ) {
        DAS_PROFILE_NODE
        auto pStruct = (char **) subexpr->evalPtr(context);
        pStruct = pStruct + total - 1;
        for ( uint32_t i=0; i!=total; ++i, pStruct-- ) {
            if ( *pStruct ) {
                context.heap.free(*pStruct, structSize);
                *pStruct = nullptr;
            }
        }
        return v_zero();
    }

    vec4f SimNode_Swizzle::eval ( Context & context ) {
        DAS_PROFILE_NODE
        union {
            vec4f   res;
            float   val[4];
        } R, S;
        S.res = value->eval(context);
        R.val[0] = S.val[fields[0]];
        R.val[1] = S.val[fields[1]];
        R.val[2] = S.val[fields[2]];
        R.val[3] = S.val[fields[3]];
        return R.res;
    }

    // SimNode_MakeBlock

    vec4f SimNode_MakeBlock::eval ( Context & context )  {
        DAS_PROFILE_NODE
        Block * block = (Block *) ( context.stack.sp() + stackTop );
        block->stackOffset = context.stack.spi();
        block->argumentsOffset = argStackTop ? (context.stack.spi() + argStackTop) : 0;
        block->body = subexpr;
        block->functionArguments = context.abiArguments();
        return cast<Block *>::from(block);
    }

    // SimNode_Debug

    void das_debug ( Context * context, TypeInfo * typeInfo, const char * FILE, int LINE, vec4f res, const char * message ) {
        TextWriter ssw;
        if ( message ) ssw << message << " ";
        ssw << debug_type(typeInfo) << " = " << debug_value(res, typeInfo, PrintFlags::debugger)
        << " at " << FILE << ":" << LINE << "\n";
        context->to_out(ssw.str().c_str());
    }

    vec4f SimNode_Debug::eval ( Context & context ) {
        DAS_PROFILE_NODE
        vec4f res = subexpr->eval(context);
        TextWriter ssw;
        if ( message ) ssw << message << " ";
        ssw << debug_type(typeInfo) << " = " << debug_value(res, typeInfo, PrintFlags::debugger)
            << " at " << debugInfo.describe() << "\n";
        context.to_out(ssw.str().c_str());
        return res;
    }

    // SimNode_Assert

    vec4f SimNode_Assert::eval ( Context & context ) {
        DAS_PROFILE_NODE
        if ( !subexpr->evalBool(context) ) {
            string error_message = "assert failed";
            if ( message )
                error_message = error_message + ", " + message;
            string error = reportError(debugInfo, error_message );
            error = context.getStackWalk(false) + error;
            context.to_err(error.c_str());
            context.throw_error_at(debugInfo,"assert failed");
        }
        return v_zero();
    }

    // SimNode_TryCatch

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4611)
#endif

    vec4f SimNode_TryCatch::eval ( Context & context ) {
        DAS_PROFILE_NODE
        auto aa = context.abiArg; auto acm = context.abiCMRES;
        char * EP, * SP;
        context.stack.watermark(EP,SP);
        #if DAS_ENABLE_EXCEPTIONS
            try {
                try_block->eval(context);
            } catch ( const dasException & ) {
                context.abiArg = aa;
                context.abiCMRES = acm;
                context.stack.pop(EP,SP);
                context.stopFlags = 0;
                catch_block->eval(context);
            }
        #else
            jmp_buf ev;
            jmp_buf * JB = context.throwBuf;
            context.throwBuf = &ev;
            if ( !setjmp(ev) ) {
                try_block->eval(context);
            } else {
                context.throwBuf = JB;
                context.abiArg = aa;
                context.abiCMRES = acm;
                context.stack.pop(EP,SP);
                context.stopFlags = 0;
                context.exception = nullptr;
                catch_block->eval(context);
            }
            context.throwBuf = JB;
        #endif
        return v_zero();
    }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

    // SimNode_CopyRefValue

    vec4f SimNode_CopyRefValue::eval ( Context & context ) {
        DAS_PROFILE_NODE
        auto pl = l->evalPtr(context);
        auto pr = r->evalPtr(context);
        memcpy ( pl, pr, size );
        return v_zero();
    }

    // SimNode_MoveRefValue

    vec4f SimNode_MoveRefValue::eval ( Context & context ) {
        DAS_PROFILE_NODE
        auto pl = l->evalPtr(context);
        auto pr = r->evalPtr(context);
        memcpy ( pl, pr, size );
        memset ( pr, 0, size );
        return v_zero();
    }

    // SimNode_CallBase

    SimNode * SimNode_CallBase::copyNode ( Context & context, NodeAllocator * code ) {
        SimNode_CallBase * that = (SimNode_CallBase *) SimNode::copyNode(context, code);
        if ( nArguments ) {
            SimNode ** newArguments = (SimNode **) code->allocate(nArguments * sizeof(SimNode *));
            memcpy ( newArguments, that->arguments, nArguments * sizeof(SimNode *));
            that->arguments = newArguments;
            if ( that->types ) {
                TypeInfo ** newTypes = (TypeInfo **) code->allocate(nArguments * sizeof(TypeInfo **));
                memcpy ( newTypes, that->types, nArguments * sizeof(TypeInfo **));
                that->types = newTypes;
            }
        }
        if ( fnPtr ) {
            that->fnPtr = context.getFunction(fnIndex);
        }
        return that;
    }

    // SimNode_Final

    SimNode * SimNode_Final::copyNode ( Context & context, NodeAllocator * code ) {
        SimNode_Final * that = (SimNode_Final *) SimNode::copyNode(context, code);
        if ( totalFinal ) {
            SimNode ** newList = (SimNode **) code->allocate(totalFinal * sizeof(SimNode *));
            memcpy ( newList, that->finalList, totalFinal*sizeof(SimNode *));
            that->finalList = newList;
        }
        return that;
    }

    // SimNode_Block

    SimNode * SimNode_Block::copyNode ( Context & context, NodeAllocator * code ) {
        SimNode_Block * that = (SimNode_Block *) SimNode_Final::copyNode(context, code);
        if ( total ) {
            SimNode ** newList = (SimNode **) code->allocate(total * sizeof(SimNode *));
            memcpy ( newList, that->list, total*sizeof(SimNode *));
            that->list = newList;
        }
        if ( totalLabels ) {
            uint32_t * newLabels = (uint32_t *) code->allocate(totalLabels * sizeof(uint32_t));
            memcpy ( newLabels, that->labels, totalLabels*sizeof(uint32_t));
            that->labels = newLabels;
        }
        return that;
    }

    vec4f SimNode_Block::eval ( Context & context ) {
        DAS_PROFILE_NODE
        SimNode ** __restrict tail = list + total;
        for (SimNode ** __restrict body = list; body!=tail; ++body) {
            (*body)->eval(context);
            if ( context.stopFlags ) break;
        }
        evalFinal(context);
        return v_zero();
    }

    vec4f SimNode_BlockNF::eval ( Context & context ) {
        DAS_PROFILE_NODE
        SimNode ** __restrict tail = list + total;
        for (SimNode ** __restrict body = list; body!=tail; ++body) {
            (*body)->eval(context);
            if ( context.stopFlags ) break;
        }
        return v_zero();
    }

    vec4f SimNode_ClosureBlock::eval ( Context & context ) {
        DAS_PROFILE_NODE
        SimNode ** __restrict tail = list + total;
        for (SimNode ** __restrict body = list; body!=tail; ++body) {
            (*body)->eval(context);
            if ( context.stopFlags ) break;
        }
        evalFinal(context);
        if ( context.stopFlags & EvalFlags::stopForReturn ) {
            context.stopFlags &= ~EvalFlags::stopForReturn;
            return context.abiResult();
        } else {
            if ( needResult ) context.throw_error_at(debugInfo,"end of block without return");
            return v_zero();
        }
    }

// SimNode_BlockWithLabels

    vec4f SimNode_BlockWithLabels::eval ( Context & context ) {
        DAS_PROFILE_NODE
        SimNode ** __restrict tail = list + total;
        SimNode ** __restrict body = list;
    loopbegin:;
        for (; body!=tail; ++body) {
            (*body)->eval(context);
            { if ( context.stopFlags ) {
                if (context.stopFlags&EvalFlags::jumpToLabel && context.gotoLabel<totalLabels) {
                    if ((body=list+labels[context.gotoLabel])>=list) {
                        context.stopFlags &= ~EvalFlags::jumpToLabel;
                        goto loopbegin;
                    }
                }
                goto loopend;
            } }
        }
    loopend:;
        evalFinal(context);
        return v_zero();
    }

    // SimNode_Let

    vec4f SimNode_Let::eval ( Context & context ) {
        DAS_PROFILE_NODE
        for ( uint32_t i = 0; i!=total && !context.stopFlags; ) {
            list[i++]->eval(context);
        }
        return v_zero();
    }

    // SimNode_While

    vec4f SimNode_While::eval ( Context & context ) {
        DAS_PROFILE_NODE
        SimNode ** __restrict tail = list + total;
        while ( cond->evalBool(context) && !context.stopFlags ) {
            SimNode ** __restrict body = list;
        loopbegin:;
            for (; body!=tail; ++body) {
                (*body)->eval(context);
                DAS_PROCESS_LOOP_FLAGS(break);
            }
        }
    loopend:;
        evalFinal(context);
        context.stopFlags &= ~EvalFlags::stopForBreak;
        return v_zero();
    }

    // Return

    vec4f SimNode_Return::eval ( Context & context ) {
        DAS_PROFILE_NODE
        if ( subexpr ) context.abiResult() = subexpr->eval(context);
        context.stopFlags |= EvalFlags::stopForReturn;
        return v_zero();
    }

    vec4f SimNode_ReturnNothing::eval ( Context & context ) {
        DAS_PROFILE_NODE
        context.stopFlags |= EvalFlags::stopForReturn;
        return v_zero();
    }

    vec4f SimNode_ReturnConst::eval ( Context & context ) {
        DAS_PROFILE_NODE
        context.abiResult() = value;
        context.stopFlags |= EvalFlags::stopForReturn;
        return v_zero();
    }

    vec4f SimNode_ReturnConstString::eval ( Context & context ) {
        DAS_PROFILE_NODE
        context.abiResult() = cast<char *>::from(value);
        context.stopFlags |= EvalFlags::stopForReturn;
        return v_zero();
    }

    vec4f SimNode_ReturnRefAndEval::eval ( Context & context ) {
        DAS_PROFILE_NODE
        auto pl = context.abiCopyOrMoveResult();
        DAS_ASSERT(pl);
        auto pR = ((char **)(context.stack.sp() + stackTop));
        *pR = pl;
        subexpr->evalPtr(context);;
        context.abiResult() = cast<char *>::from(pl);
        context.stopFlags |= EvalFlags::stopForReturn;
        return v_zero();
    }

    vec4f SimNode_ReturnAndCopy::eval ( Context & context ) {
        DAS_PROFILE_NODE
        auto pr = subexpr->evalPtr(context);
        auto pl = context.abiCopyOrMoveResult();
        DAS_ASSERT(pl);
        memcpy ( pl, pr, size);
        context.abiResult() = cast<char *>::from(pl);
        context.stopFlags |= EvalFlags::stopForReturn;
        return v_zero();
    }

    vec4f SimNode_ReturnAndMove::eval ( Context & context ) {
        DAS_PROFILE_NODE
        auto pr = subexpr->evalPtr(context);
        auto pl = context.abiCopyOrMoveResult();
        DAS_ASSERT(pl);
        memcpy ( pl, pr, size);
        memset ( pr, 0, size);
        context.abiResult() = cast<char *>::from(pl);
        context.stopFlags |= EvalFlags::stopForReturn;
        return v_zero();
    }

    vec4f SimNode_ReturnReference::eval ( Context & context ) {
        DAS_PROFILE_NODE
        char * ref = subexpr->evalPtr(context);
        if ( context.stack.bottom()<=ref && ref<context.stack.sp()) {
            context.throw_error_at(debugInfo,"reference bellow current function stack frame");
            return v_zero();
        }
#if DAS_ENABLE_STACK_WALK
        auto pp = (Prologue *) context.stack.sp();
        auto top = context.stack.sp() + pp->info->stackSize;
        if ( context.stack.sp()<=ref && ref<top ) {
            context.throw_error_at(debugInfo,"reference to current function stack frame");
            return v_zero();
        }
#endif
        context.abiResult() = cast<char *>::from(ref);
        context.stopFlags |= EvalFlags::stopForReturn;
        return v_zero();
    }

    vec4f SimNode_ReturnRefAndEvalFromBlock::eval ( Context & context ) {
        DAS_PROFILE_NODE
        auto ba = (BlockArguments *) ( context.stack.sp() + argStackTop );
        auto pl = ba->copyOrMoveResult;
        DAS_ASSERT(pl);
        auto pR = ((char **)(context.stack.sp() + stackTop));
        *pR = pl;
        subexpr->evalPtr(context);;
        context.abiResult() = cast<char *>::from(pl);
        context.stopFlags |= EvalFlags::stopForReturn;
        return v_zero();
    }

    vec4f SimNode_ReturnAndCopyFromBlock::eval ( Context & context ) {
        DAS_PROFILE_NODE
        auto pr = subexpr->evalPtr(context);
        auto ba = (BlockArguments *) ( context.stack.sp() + argStackTop );
        auto pl = ba->copyOrMoveResult;
        memcpy ( pl, pr, size);
        context.abiResult() = cast<char *>::from(pl);
        context.stopFlags |= EvalFlags::stopForReturn;
        return v_zero();
    }

    vec4f SimNode_ReturnAndMoveFromBlock::eval ( Context & context ) {
        DAS_PROFILE_NODE
        auto pr = subexpr->evalPtr(context);
        auto ba = (BlockArguments *) ( context.stack.sp() + argStackTop );
        auto pl = ba->copyOrMoveResult;
        memcpy ( pl, pr, size);
        memset ( pr, 0, size);
        context.abiResult() = cast<char *>::from(pl);
        context.stopFlags |= EvalFlags::stopForReturn;
        return v_zero();
    }

    vec4f SimNode_ReturnReferenceFromBlock::eval ( Context & context ) {
        DAS_PROFILE_NODE
        char * ref = subexpr->evalPtr(context);
        if ( context.stack.bottom()<=ref && ref<context.stack.ap() ) {
            context.throw_error_at(debugInfo,"reference bellow current call chain stack frame");
            return v_zero();
        }
        context.abiResult() = cast<char *>::from(ref);
        context.stopFlags |= EvalFlags::stopForReturn;
        return v_zero();
    }

    vec4f SimNode_ReturnLocalCMRes::eval ( Context & context ) {
        DAS_PROFILE_NODE
        SimNode ** __restrict tail = list + total;
        SimNode ** __restrict body = list;
        for (; body!=tail; ++body) {
            (*body)->eval(context);
            if (context.stopFlags) break;
        }
        context.abiResult() = cast<char *>::from(context.abiCopyOrMoveResult());
        context.stopFlags |= EvalFlags::stopForReturn;
        return v_zero();
    }

    // Context

    Context::Context(uint32_t stackSize) : stack(stackSize) {
        code = make_shared<NodeAllocator>();
        constStringHeap = make_shared<StringAllocator>();
        debugInfo = make_shared<DebugInfoAllocator>();
    }

    Context::Context(const Context & ctx) : stack(ctx.stack.size()) {
        code = ctx.code;
        constStringHeap = ctx.constStringHeap;
        debugInfo = ctx.debugInfo;
        thisProgram = ctx.thisProgram;
        thisHelper = ctx.thisHelper;
        // globals
        annotationData = ctx.annotationData;
        globalsSize = ctx.globalsSize;
        globalInitStackSize = ctx.globalInitStackSize;
        globalVariables = ctx.globalVariables;
        totalVariables = ctx.totalVariables;
        if ( ctx.globals ) {
            globals = (char *) das_aligned_alloc16(globalsSize);
        }
        // functoins
        functions = ctx.functions;
        totalFunctions = ctx.totalFunctions;
        // mangled name table
        tabMnLookup = ctx.tabMnLookup;
        tabMnMask = ctx.tabMnMask;
        tabMnRot = ctx.tabMnRot;
        // annotation data table
        tabAdLookup = ctx.tabAdLookup;
        tabAdMask = ctx.tabAdMask;
        tabAdRot = ctx.tabAdRot;
        // now, make it good to go
        restart();
        runInitScript();
        restart();
    }

    Context::~Context() {
        if ( globals ) {
            das_aligned_free16(globals);
        }
    }

    uint64_t Context::getSharedMemorySize() const {
        uint64_t mem = 0;
        mem += code->totalAlignedMemoryAllocated();
        mem += constStringHeap->totalAlignedMemoryAllocated();
        mem += debugInfo->totalAlignedMemoryAllocated();
        return mem;
    }

    uint64_t Context::getUniqueMemorySize() const {
        uint64_t mem = 0;
        mem += globalsSize;
        mem += stack.size();
        mem += heap.totalAlignedMemoryAllocated();
        mem += stringHeap.totalAlignedMemoryAllocated();
        return mem;
    }

    struct SimNodeRelocator : SimVisitor {
        shared_ptr<NodeAllocator>   newCode;
        Context * context = nullptr;
        virtual SimNode * visit ( SimNode * node ) override {
            return node->copyNode(*context, newCode.get());
        }
    };

    void Context::relocateCode() {
        SimNodeRelocator rel;
        rel.context = this;
        rel.newCode = make_shared<NodeAllocator>();
        rel.newCode->prefixWithHeader = false;
        uint32_t codeSize = code->bytesAllocated() - code->totalNodesAllocated * uint32_t(sizeof(NodePrefix));
        rel.newCode->pageSize = (codeSize + rel.newCode->alignMask) & ~rel.newCode->alignMask;
        rel.newCode->setInitialSize(codeSize);
        if ( totalFunctions ) {
            SimFunction * newFunctions = (SimFunction *) rel.newCode->allocate(totalFunctions*sizeof(SimFunction));
            memcpy ( newFunctions, functions, totalFunctions*sizeof(SimFunction));
            for ( int i=0; i!=totalFunctions; ++i ) {
                newFunctions[i].name = rel.newCode->allocateName(functions[i].name);
                newFunctions[i].mangledName = rel.newCode->allocateName(functions[i].mangledName);
            }
            functions = newFunctions;
        }
        if ( totalVariables ) {
            GlobalVariable * newVariables = (GlobalVariable *) rel.newCode->allocate(totalVariables*sizeof(GlobalVariable));
            memcpy ( newVariables, globalVariables, totalVariables*sizeof(GlobalVariable));
            for ( int i=0; i!=totalVariables; ++i ) {
                newVariables[i].name = rel.newCode->allocateName(globalVariables[i].name);
            }
            globalVariables = newVariables;
        }
        // relocate functions
        for ( int i=0; i!=totalFunctions; ++i ) {
            auto & fn = functions[i];
            fn.code = fn.code->visit(rel);
        }
        // relocate variables
        if ( totalVariables ) {
            for ( int j=0; j!=totalVariables; ++j ) {
                auto & var = globalVariables[j];
                if ( var.init) {
                    var.init = var.init->visit(rel);
                }
            }
        }
        // relocate mangle-name lookup
        if ( tabMnLookup ) {
            uint32_t * newMnLookup = (uint32_t *) rel.newCode->allocate(tabMnSize * sizeof(uint32_t));
            memcpy ( newMnLookup, tabMnLookup, tabMnSize * sizeof(uint32_t));
            tabMnLookup = newMnLookup;
        }
        // relocate annotation data lookup
        if ( tabAdLookup ) {
            uint64_t * newAdLookup = (uint64_t *) rel.newCode->allocate(tabAdSize * sizeof(uint64_t));
            memcpy ( newAdLookup, tabAdLookup, tabAdSize * sizeof(uint64_t));
            tabAdLookup = newAdLookup;
        }
        // swap the code
        DAS_ASSERTF(rel.newCode->pagesAllocated()<=1,"after code relocation all code should be on one page");
        code = rel.newCode;
    }

    void Context::runInitScript ( void ) {
        DAS_ASSERTF(insideContext==0,"can't run init script on the locked context");
        char * EP, *SP;
        if(!stack.push(globalInitStackSize,EP,SP)) {
            throw_error("stack overflow in the initialization script");
            return;
        }
        if ( aotInitScript ) {
            vec4f args[1] = { cast<void *>::from(this) };
            abiArg = args;
            abiCMRES = nullptr;
            aotInitScript->eval(*this);
        } else {

#if DAS_ENABLE_STACK_WALK
            FuncInfo finfo;
            memset(&finfo, 0, sizeof(finfo));
            finfo.name = (char *) "Context::runInitScript";
#endif
            for ( int i=0; i!=totalVariables && !stopFlags; ++i ) {
                auto & pv = globalVariables[i];
                if ( pv.init ) {
#if DAS_ENABLE_STACK_WALK
                    finfo.stackSize = globalInitStackSize;

                    Prologue * pp = (Prologue *)stack.sp();
                    pp->arguments = nullptr;
                    pp->info = &finfo;
                    pp->line = 0;
#endif
                    pv.init->eval(*this);
                } else {
                    memset ( globals + pv.offset, 0, pv.size );
                }
            }
            for ( int j=0; j!=totalFunctions && !stopFlags; ++j ) {
                auto & pf = functions[j];
                if ( pf.debugInfo->flags & FuncInfo::flag_init ) {
                    call(&pf, nullptr, 0);
                }

            }
        }
        stack.pop(EP,SP);
    }

    SimFunction * Context::findFunction ( const char * name ) const {
        for ( int fni = 0; fni != totalFunctions; ++fni ) {
            if ( strcmp(functions[fni].name, name)==0 ) {
                return functions + fni;
            }
        }
        return nullptr;
    }

    int Context::findVariable ( const char * name ) const {
        for ( int vni = 0; vni != totalVariables; ++vni ) {
            if ( strcmp(globalVariables[vni].name, name)==0 ) {
                return vni;
            }
        }
        return -1;
    }

    void Context::stackWalk() {
        auto str = getStackWalk();
        to_out(str.c_str());
    }

    string Context::getStackWalk( bool args ) {
        TextWriter ssw;
    #if DAS_ENABLE_STACK_WALK
        ssw << "\nCALL STACK (sp=" << (stack.top() - stack.sp()) << "):\n";
        char * sp = stack.sp();
        while (  sp < stack.top() ) {
            Prologue * pp = (Prologue *) sp;
            if ( pp->line ) {
                ssw << pp->info->name << " at line " << pp->line << " (sp=" << (stack.top() - sp) << ")\n";
            } else {
                ssw << pp->info->name << "(sp=" << (stack.top() - sp) << ")\n";
            }
            if ( args ) {
                for ( uint32_t i = 0; i != pp->info->count; ++i ) {
                    ssw << "\t" << pp->info->fields[i]->name
                        << " : " << debug_type(pp->info->fields[i])
                        << " = \t" << debug_value(pp->arguments[i], pp->info->fields[i], PrintFlags::stackwalker) << "\n";
                }
            }
            sp += pp->info->stackSize;
        }
        ssw << "\n";
    #else
        ssw << "\nCALL STACK TRACKING DISABLED:\n\n";
    #endif
        return ssw.str();
    }

    void Context::breakPoint(int, int ) const {
        os_debug_break();
    }

    void Context::to_out ( const char * message ) {
		if (message) {
			fprintf(stdout, "%s", message);
			fflush(stdout);
		}
    }

    void Context::to_err ( const char * message ) {
		if (message) {
			fprintf(stderr, "%s", message);
			fflush(stderr);
		}
    }

    void Context::throw_error_at ( const LineInfo & at, const char * message, ... ) {
        const int PRINT_BUFFER_SIZE = 1024;
        char buffer[PRINT_BUFFER_SIZE];
        va_list args;
        va_start (args, message);
        vsnprintf (buffer,PRINT_BUFFER_SIZE,message, args);
        va_end (args);
        lastError = at.describe() + ": " + buffer;
        throw_error(lastError.c_str());
    }

    void Context::throw_error_ex ( const char * message, ... ) {
        const int PRINT_BUFFER_SIZE = 1024;
        char buffer[PRINT_BUFFER_SIZE];
        va_list args;
        va_start (args, message);
        vsnprintf (buffer,PRINT_BUFFER_SIZE,message, args);
        va_end (args);
        lastError = buffer;
        throw_error(lastError.c_str());
    }

    void Context::throw_error ( const char * message ) {
        exception = message;
#if DAS_ENABLE_EXCEPTIONS
        throw dasException(message ? message : "");
#else
        if ( throwBuf ) {
            longjmp(*throwBuf,1);
        } else {
            to_err("\nunhandled exception\n");
            if ( exception ) {
                to_err(exception);
                to_err("\n");
            }
            stackWalk();
            os_debug_break();
        }
#endif
    }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4611)
#endif

    vec4f Context::evalWithCatch ( SimNode * node ) {
        auto aa = abiArg; auto acm = abiCMRES;
        char * EP, * SP;
        stack.watermark(EP,SP);
#if DAS_ENABLE_EXCEPTIONS
        try {
            return node->eval(*this);
        } catch ( const dasException & ) {
            /*
             to_err("\nunhandled exception\n");
             if ( exception ) {
             to_err(exception);
             to_err("\n");
             }
             stackWalk();
             */
            abiArg = aa; abiCMRES = acm;
            stack.pop(EP,SP);
            return v_zero();
        }
#else
        jmp_buf ev;
        jmp_buf * JB = throwBuf;
        throwBuf = &ev;
        if ( !setjmp(ev) ) {
            return node->eval(*this);
        } else {
            abiArg = aa;
            abiCMRES = acm;
            stack.pop(EP,SP);
            throwBuf = JB;
            return v_zero();
        }
        throwBuf = JB;
#endif
    }

    bool Context::runWithCatch ( const function<void()> & subexpr ) {
        auto aa = abiArg; auto acm = abiCMRES;
        char * EP, * SP;
        stack.watermark(EP,SP);
#if DAS_ENABLE_EXCEPTIONS
        try {
            subexpr();
            return true;
        } catch ( const dasException & ) {
            /*
             to_err("\nunhandled exception\n");
             if ( exception ) {
             to_err(exception);
             to_err("\n");
             }
             stackWalk();
             */
            abiArg = aa; abiCMRES = acm;
            stack.pop(EP,SP);
            return false;
        }
#else
        jmp_buf ev;
        jmp_buf * JB = throwBuf;
        throwBuf = &ev;
        if ( !setjmp(ev) ) {
            subexpr();
        } else {
            abiArg = aa;
            abiCMRES = acm;
            stack.pop(EP,SP);
            throwBuf = JB;
            return false;
        }
        throwBuf = JB;
        return true;
#endif
    }

    vec4f Context::evalWithCatch ( SimFunction * fnPtr, vec4f * args, void * res ) {
        auto aa = abiArg; auto acm = abiCMRES;
        char * EP, * SP;
        stack.watermark(EP,SP);
#if DAS_ENABLE_EXCEPTIONS
        try {
            return call(fnPtr, args, res, 0);
        } catch ( const dasException & ) {
            /*
            to_err("\nunhandled exception\n");
            if ( exception ) {
                to_err(exception);
                to_err("\n");
            }
            stackWalk();
            */
            abiArg = aa; abiCMRES = acm;
            stack.pop(EP,SP);
            return v_zero();
        }
#else
        jmp_buf ev;
        jmp_buf * JB = throwBuf;
        throwBuf = &ev;
        if ( !setjmp(ev) ) {
            return callWithCopyOnReturn(fnPtr, args, res, 0);
        } else {
            abiArg = aa;
            abiCMRES = acm;
            stack.pop(EP,SP);
            throwBuf = JB;
            return v_zero();
        }
        throwBuf = JB;
#endif
    }

    void Context::resetProfiler() {
#if DAS_ENABLE_PROFILER
        profileData.clear();
#endif
    }

    void Context::dumpProfileInfo() {
        TextPrinter tout;
#if DAS_ENABLE_PROFILER
        uint64_t totalGoo = 0;
        set<FileInfo *> allFiles;
        for ( auto & it : profileData ) {
            totalGoo += it.second;
            auto info = it.first.fileInfo;
            if ( info && info->source ) {
                allFiles.insert(info);
            }
        }
        tout << "\nPROFILING RESULTS:\n";
        for ( auto fi : allFiles ) {
            tout << fi->name << "\n";
            LineInfo info;
            info.fileInfo = fi;
            info.column = 0;
            bool newLine = true;
            int  line = 0;
            char txt[2];
            txt[1] = 0;
            int col = 0;
            for ( uint32_t i = 0; i!=fi->sourceLength; ++i ) {
                if ( newLine ) {
                    line ++;
                    col = 0;
                    newLine = false;
                    info.line = line;
                    char total[20];
                    auto itI = profileData.find(info);
                    if ( itI != profileData.end() ) {
                        uint64_t samples = uint64_t(itI->second);
                        snprintf(total, 20, "%-6.2f", samples*100.1/totalGoo);
                        tout << total;
                    } else {
                        tout << "      ";
                    }
                }
                txt[0] = fi->source[i];
                if (txt[0] == '\n') {
                    newLine = true;
                }
                if (txt[0] == '\t') {
                    if (col % 4 == 0) {
                        tout << "    ";
                        col += 4;
                    } else {
                        while (col % 4) {
                            tout << " ";
                            col++;
                        }
                    }
                } else {
                    tout << txt;
                    col++;
                }
            }
        }

#else
        tout << "\nPROFILIER IS DISABLED\n";
#endif
    }

#ifdef _MSC_VER
#pragma warning(pop)
#endif
}

//workaround compiler bug in MSVC 32 bit
#if defined(_MSC_VER) && INTPTR_MAX == INT32_MAX
vec4i VECTORCALL v_splats_ptr(const void * a) {return v_splatsi((int32_t)a);}
#endif
