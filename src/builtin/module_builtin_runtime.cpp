#include "daScript/misc/platform.h"

#include "module_builtin.h"

#include "daScript/ast/ast_interop.h"
#include "daScript/simulate/aot_builtin.h"
#include "daScript/simulate/runtime_profile.h"
#include "daScript/simulate/hash.h"
#include "daScript/simulate/bin_serializer.h"
#include "daScript/simulate/runtime_array.h"
#include "daScript/simulate/runtime_range.h"

namespace das
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif

    struct MarkFunctionAnnotation : FunctionAnnotation {
        MarkFunctionAnnotation(const string & na) : FunctionAnnotation(na) { }
        virtual bool apply(ExprBlock *, ModuleGroup &, const AnnotationArgumentList &, string & err) override {
            err = "not supported for block";
            return false;
        }
        virtual bool finalize(ExprBlock *, ModuleGroup &,const AnnotationArgumentList &, const AnnotationArgumentList &, string &) override {
            return true;
        }
        virtual bool finalize(const FunctionPtr &, ModuleGroup &, const AnnotationArgumentList &, const AnnotationArgumentList &, string &) override {
            return true;
        }
    };

    struct PrivateFunctionAnnotation : MarkFunctionAnnotation {
        PrivateFunctionAnnotation() : MarkFunctionAnnotation("private") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->privateFunction = true;
            return true;
        };
    };

    struct UnsafeDerefFunctionAnnotation : MarkFunctionAnnotation {
        UnsafeDerefFunctionAnnotation() : MarkFunctionAnnotation("unsafe_deref") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->unsafeDeref = true;
            return true;
        };
    };

    struct GenericFunctionAnnotation : MarkFunctionAnnotation {
        GenericFunctionAnnotation() : MarkFunctionAnnotation("generic") { }
        virtual bool isGeneric() const override {
            return true;
        }
        virtual bool apply(const FunctionPtr &, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            return true;
        };
    };

    struct ExportFunctionAnnotation : MarkFunctionAnnotation {
        ExportFunctionAnnotation() : MarkFunctionAnnotation("export") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->exports = true;
            return true;
        };
    };

    struct SideEffectsFunctionAnnotation : MarkFunctionAnnotation {
        SideEffectsFunctionAnnotation() : MarkFunctionAnnotation("sideeffects") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->sideEffectFlags |= uint32_t(SideEffects::userScenario);
            return true;
        };
    };
    
    struct RunAtCompileTimeFunctionAnnotation : MarkFunctionAnnotation {
        RunAtCompileTimeFunctionAnnotation() : MarkFunctionAnnotation("run") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->hasToRunAtCompileTime = true;
            return true;
        };
    };

    struct UnsafeOpFunctionAnnotation : MarkFunctionAnnotation {
        UnsafeOpFunctionAnnotation() : MarkFunctionAnnotation("unsafe_operation") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->unsafeOperation = true;
            return true;
        };
    };

    struct UnsafeFunctionAnnotation : MarkFunctionAnnotation {
        UnsafeFunctionAnnotation() : MarkFunctionAnnotation("unsafe") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->unsafe = true;
            return true;
        };
    };

    struct NoAotFunctionAnnotation : MarkFunctionAnnotation {
        NoAotFunctionAnnotation() : MarkFunctionAnnotation("no_aot") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->noAot = true;
            return true;
        };
    };

    struct InitFunctionAnnotation : MarkFunctionAnnotation {
        InitFunctionAnnotation() : MarkFunctionAnnotation("init") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->init = true;
            return true;
        };
        virtual bool finalize(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, const AnnotationArgumentList &, string & errors) override {
            if ( func->arguments.size() ) {
                errors += "[init] function can't have any arguments";
                return false;
            }
            if ( !func->result->isVoid() ) {
                errors += "[init] function can't return value";
                return false;
            }
            return true;
        }
    };

    // totally dummy annotation, needed for comments
    struct CommentAnnotation : StructureAnnotation {
        CommentAnnotation() : StructureAnnotation("comment") {}
        virtual bool touch(const StructurePtr &, ModuleGroup &,
                           const AnnotationArgumentList &, string & ) override {
            return true;
        }
        virtual bool look ( const StructurePtr &, ModuleGroup &,
                           const AnnotationArgumentList &, string & ) override {
            return true;
        }
    };

    struct HybridFunctionAnnotation : MarkFunctionAnnotation {
        HybridFunctionAnnotation() : MarkFunctionAnnotation("hybrid") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->aotHybrid = true;
            return true;
        };
    };

    struct CppAlignmentAnnotation : StructureAnnotation {
        CppAlignmentAnnotation() : StructureAnnotation("cpp_layout") {}
        virtual bool touch(const StructurePtr & ps, ModuleGroup &,
                           const AnnotationArgumentList & args, string & ) override {
            ps->cppLayout = true;
            ps->cppLayoutPod = args.getBoolOption("pod", true);
            return true;
        }
        virtual bool look ( const StructurePtr &, ModuleGroup &,
                           const AnnotationArgumentList &, string & ) override {
            return true;
        }
    };

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

    // core functions

    void builtin_throw ( char * text, Context * context ) {
        context->throw_error(text);
    }

    void builtin_print ( char * text, Context * context ) {
        context->to_out(text);
    }

    vec4f builtin_breakpoint ( Context & context, SimNode_CallBase * call, vec4f * ) {
        context.breakPoint(call->debugInfo.column, call->debugInfo.line);
        return v_zero();
    }

    void builtin_stackwalk ( Context * context) {
        context->stackWalk();
    }

    void builtin_terminate ( Context * context ) {
        context->throw_error("terminate");
    }

    int builtin_table_size ( const Table & arr ) {
        return arr.size;
    }

    int builtin_table_capacity ( const Table & arr ) {
        return arr.capacity;
    }

    void builtin_table_clear ( Table & arr, Context * context ) {
        table_clear(*context, arr);
    }

    vec4f _builtin_hash ( Context & context, SimNode_CallBase * call, vec4f * args ) {
        auto uhash = hash_value(context, args[0], call->types[0]);
        return cast<uint32_t>::from(uhash);
    }

    uint32_t heap_bytes_allocated ( Context * context ) {
        return context->heap.bytesAllocated();
    }

    uint32_t heap_high_watermark ( Context * context ) {
        return (int32_t) context->heap.maxBytesAllocated();
    }

    int32_t heap_depth ( Context * context ) {
        return (int32_t) context->heap.shelf.size();
    }

    uint32_t string_heap_bytes_allocated ( Context * context ) {
        return context->stringHeap.bytesAllocated();
    }

    uint32_t string_heap_high_watermark ( Context * context ) {
        return (int32_t) context->stringHeap.maxBytesAllocated();
    }

    int32_t string_heap_depth ( Context * context ) {
        return (int32_t) context->stringHeap.shelf.size();
    }

    void string_heap_collect ( Context * context ) {
        context->collectStringHeap();
    }

    void string_heap_report ( Context * context ) {
        context->stringHeap.reportAllocations();
    }

    void builtin_table_lock ( const Table & arr, Context * context ) {
        table_lock(*context, const_cast<Table&>(arr));
    }

    void builtin_table_unlock ( const Table & arr, Context * context ) {
        table_unlock(*context, const_cast<Table&>(arr));
    }

    bool builtin_iterator_first ( const Iterator * it, void * data, Context * context ) {
        return ((Iterator *)it)->first(*context, (char *)data);
    }

    bool builtin_iterator_next ( const Iterator * it, void * data, Context * context ) {
        return ((Iterator *)it)->next(*context, (char *)data);
    }

    void builtin_iterator_close ( const Iterator * it, void * data, Context * context ) {
        ((Iterator *)it)->close(*context, (char *)&data);
    }

    Iterator * builtin_make_good_array_iterator ( const Array & arr, int stride, Context * context ) {
        char * iter = context->heap.allocate(sizeof(GoodArrayIterator));
        new (iter) GoodArrayIterator((Array *)&arr, stride);
        return (Iterator *) iter;
    }

    Iterator * builtin_make_fixed_array_iterator ( void * data, int size, int stride, Context * context ) {
        char * iter = context->heap.allocate(sizeof(FixedArrayIterator));
        new (iter) FixedArrayIterator((char *)data, size, stride);
        return (Iterator *) iter;
    }

    Iterator * builtin_make_range_iterator ( range rng, Context * context ) {
        char * iter = context->heap.allocate(sizeof(RangeIterator));
        new (iter) RangeIterator(rng);
        return (Iterator *) iter;
    }

    struct NilIterator : Iterator {
        virtual bool first ( Context &, char * ) override { return false; }
        virtual bool next  ( Context &, char * ) override { return false; }
        virtual void close ( Context &, char * ) override { }
    };

    Iterator * builtin_make_nil_iterator ( Context * context ) {
        char * iter = context->heap.allocate(sizeof(NilIterator));
        new (iter) NilIterator();
        return (Iterator *) iter;
    }

    struct LambdaIterator : Iterator {
        LambdaIterator ( Context & context, const Lambda & ll ) : lambda(ll) {
            int32_t * fnIndex = (int32_t *) lambda.capture;
            if (!fnIndex) context.throw_error("invoke null lambda");
            simFunc = context.getFunction(*fnIndex-1);
            if (!simFunc) context.throw_error("invoke null function");
        }
        __forceinline bool InvokeLambda ( Context & context, char * ptr ) {
            vec4f argValues[4] = {
                cast<Lambda>::from(lambda),
                cast<char *>::from(ptr)
            };
            auto res = context.call(simFunc, argValues, 0);
            return cast<bool>::to(res);
        }
        virtual bool first ( Context & context, char * ptr ) override {
            return InvokeLambda(context, ptr);
        }
        virtual bool next  ( Context & context, char * ptr ) override {
            return InvokeLambda(context, ptr);
        }
        virtual void close ( Context &, char * ) override {
            // TODO: release lambda?
        }
        Lambda          lambda;
        SimFunction *   simFunc = nullptr;
    };

    Iterator * builtin_make_lambda_iterator ( const Lambda lambda, Context * context ) {
        char * iter = context->heap.allocate(sizeof(LambdaIterator));
        new (iter) LambdaIterator(*context, lambda);
        return (Iterator *) iter;
    }

    void resetProfiler( Context * context ) {
        context->resetProfiler();
    }

    void dumpProfileInfo( Context * context ) {
        context->dumpProfileInfo();
    }

    void Module_BuiltIn::addRuntime(ModuleLibrary & lib) {
        // function annotations
        addAnnotation(make_shared<CommentAnnotation>());
        addAnnotation(make_shared<CppAlignmentAnnotation>());
        addAnnotation(make_shared<GenericFunctionAnnotation>());
        addAnnotation(make_shared<PrivateFunctionAnnotation>());
        addAnnotation(make_shared<ExportFunctionAnnotation>());
        addAnnotation(make_shared<SideEffectsFunctionAnnotation>());
        addAnnotation(make_shared<RunAtCompileTimeFunctionAnnotation>());
        addAnnotation(make_shared<UnsafeFunctionAnnotation>());
        addAnnotation(make_shared<UnsafeOpFunctionAnnotation>());
        addAnnotation(make_shared<NoAotFunctionAnnotation>());
        addAnnotation(make_shared<InitFunctionAnnotation>());
        addAnnotation(make_shared<HybridFunctionAnnotation>());
        addAnnotation(make_shared<UnsafeDerefFunctionAnnotation>());
        // iterator functions
        addExtern<DAS_BIND_FUN(builtin_iterator_first)>(*this, lib, "_builtin_iterator_first", SideEffects::modifyExternal, "builtin_iterator_first");
        addExtern<DAS_BIND_FUN(builtin_iterator_next)>(*this, lib,  "_builtin_iterator_next",  SideEffects::modifyExternal, "builtin_iterator_next");
        addExtern<DAS_BIND_FUN(builtin_iterator_close)>(*this, lib, "_builtin_iterator_close", SideEffects::modifyExternal, "builtin_iterator_close");
        // make-iterator functions
        addExtern<DAS_BIND_FUN(builtin_make_good_array_iterator)>(*this, lib,  "_builtin_make_good_array_iterator",
            SideEffects::modifyExternal, "builtin_make_good_array_iterator");
        addExtern<DAS_BIND_FUN(builtin_make_fixed_array_iterator)>(*this, lib,  "_builtin_make_fixed_array_iterator",
            SideEffects::modifyExternal, "builtin_make_fixed_array_iterator");
        addExtern<DAS_BIND_FUN(builtin_make_range_iterator)>(*this, lib,  "_builtin_make_range_iterator",
            SideEffects::modifyExternal, "builtin_make_range_iterator");
        addExtern<DAS_BIND_FUN(builtin_make_nil_iterator)>(*this, lib,  "_builtin_make_nil_iterator",
            SideEffects::modifyExternal, "builtin_make_nil_iterator");
        addExtern<DAS_BIND_FUN(builtin_make_lambda_iterator)>(*this, lib,  "_builtin_make_lambda_iterator",
            SideEffects::modifyExternal, "builtin_make_lambda_iterator");
        // functions
        addExtern<DAS_BIND_FUN(builtin_throw)>         (*this, lib, "panic", SideEffects::modifyExternal, "builtin_throw");
        addExtern<DAS_BIND_FUN(builtin_print)>         (*this, lib, "print", SideEffects::modifyExternal, "builtin_print");
        addExtern<DAS_BIND_FUN(builtin_terminate)> (*this, lib, "terminate", SideEffects::modifyExternal, "terminate");
        addExtern<DAS_BIND_FUN(builtin_stackwalk)> (*this, lib, "stackwalk", SideEffects::modifyExternal, "stackwalk");
        addInterop<builtin_breakpoint,void>     (*this, lib, "breakpoint", SideEffects::modifyExternal, "breakpoint");
        // profiler
        addExtern<DAS_BIND_FUN(resetProfiler)>(*this, lib, "resetProfiler", SideEffects::modifyExternal, "resetProfiler");
        addExtern<DAS_BIND_FUN(dumpProfileInfo)>(*this, lib, "dumpProfileInfo", SideEffects::modifyExternal, "dumpProfileInfo");
        // heap
        addExtern<DAS_BIND_FUN(heap_bytes_allocated)>(*this, lib, "heap_bytes_allocated",
                SideEffects::modifyExternal, "heap_bytes_allocated");
        addExtern<DAS_BIND_FUN(heap_high_watermark)>(*this, lib, "heap_high_watermark",
                SideEffects::modifyExternal, "heap_high_watermark");
        addExtern<DAS_BIND_FUN(heap_depth)>(*this, lib, "heap_depth",
                SideEffects::modifyExternal, "heap_depth");
        addExtern<DAS_BIND_FUN(string_heap_bytes_allocated)>(*this, lib, "string_heap_bytes_allocated",
                SideEffects::modifyExternal, "string_heap_bytes_allocated");
        addExtern<DAS_BIND_FUN(string_heap_high_watermark)>(*this, lib, "string_heap_high_watermark",
                SideEffects::modifyExternal, "string_heap_high_watermark");
        addExtern<DAS_BIND_FUN(string_heap_depth)>(*this, lib, "string_heap_depth",
                SideEffects::modifyExternal, "string_heap_depth");
        auto shc = addExtern<DAS_BIND_FUN(string_heap_collect)>(*this, lib, "string_heap_collect",
                SideEffects::modifyExternal, "string_heap_collect");
        shc->unsafeOperation = true;
        addExtern<DAS_BIND_FUN(string_heap_report)>(*this, lib, "string_heap_report",
                SideEffects::modifyExternal, "string_heap_report");
        // binary serializer
        addInterop<_builtin_binary_load,void,vec4f,const Array &>(*this,lib,"_builtin_binary_load",SideEffects::modifyArgument, "_builtin_binary_load");
        addInterop<_builtin_binary_save,void,const vec4f,const Block &>(*this, lib, "_builtin_binary_save",SideEffects::modifyExternal, "_builtin_binary_save");
        // function-like expresions
        addCall<ExprAssert>         ("assert",false);
        addCall<ExprAssert>         ("verify",true);
        addCall<ExprStaticAssert>   ("static_assert");
        addCall<ExprDebug>          ("debug");
        // hash
        addInterop<_builtin_hash,uint32_t,vec4f>(*this, lib, "hash", SideEffects::none, "hash");
        // table functions
        addExtern<DAS_BIND_FUN(builtin_table_clear)>(*this, lib, "clear", SideEffects::modifyArgument, "builtin_table_clear");
        addExtern<DAS_BIND_FUN(builtin_table_size)>(*this, lib, "length", SideEffects::none, "builtin_table_size");
        addExtern<DAS_BIND_FUN(builtin_table_capacity)>(*this, lib, "capacity", SideEffects::none, "builtin_table_capacity");
        addExtern<DAS_BIND_FUN(builtin_table_lock)>(*this, lib, "__builtin_table_lock", SideEffects::modifyArgument, "builtin_table_lock");
        addExtern<DAS_BIND_FUN(builtin_table_unlock)>(*this, lib, "__builtin_table_unlock", SideEffects::modifyArgument, "builtin_table_unlock");
        addExtern<DAS_BIND_FUN(builtin_table_keys)>(*this, lib, "__builtin_table_keys", SideEffects::modifyArgument, "builtin_table_keys");
        addExtern<DAS_BIND_FUN(builtin_table_values)>(*this, lib, "__builtin_table_values", SideEffects::modifyArgument, "builtin_table_values");
        // table expressions
        addCall<ExprErase>("__builtin_table_erase");
        addCall<ExprFind>("__builtin_table_find");
        addCall<ExprKeyExists>("__builtin_table_key_exists");
        // blocks
        addCall<ExprInvoke>("invoke");
        // profile
        addExtern<DAS_BIND_FUN(builtin_profile)>(*this,lib,"profile", SideEffects::modifyExternal, "builtin_profile");
        addString(lib);
    }
}
