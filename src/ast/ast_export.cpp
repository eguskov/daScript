#include "daScript/misc/platform.h"

#include "daScript/ast/ast.h"

namespace das {

	class MarkSymbolUse : public Visitor {
	public:
		void propageteVarUse(const VariablePtr & var) {
			if (var->used) return;
			var->used = true;
			auto mn = var->getMangledName();
			for (const auto & gv : gVarVarUse[mn]) {
				propageteVarUse(gv);
			}
			for (const auto & fn : gVarFuncUse[mn]) {
				propagateFunctionUse(fn);
			}
		}
		void propagateFunctionUse(const FunctionPtr & fn) {
			if (fn->used) return;
			fn->used = true;
			for (const auto & gv : fn->useGlobalVariables) {
				propageteVarUse(gv);
			}
			for (const auto & it : fn->useFunctions) {
				propagateFunctionUse(it);
			}
		}
		void markUsedFunctions( Module & thisModule ){
			for (const auto & it : thisModule.functions) {
				auto fn = it.second;
				if (fn->exports) {
					propagateFunctionUse(fn);
				}
			}
		}
        void RemoveUnusedSymbols ( Module & mod ) {
            map<string,FunctionPtr> functions;
            map<string,VariablePtr> globals;
            swap(functions,mod.functions);
            swap(globals,mod.globals);
            mod.functionsByName.clear();
            for ( auto & fn : functions ) {
                if ( fn.second->used ) {
                    mod.addFunction(fn.second);
                }
            }
            for ( auto & var : globals ) {
                if ( var.second->used ) {
                    mod.addVariable(var.second);
                }
            }
        }
	protected:
		ProgramPtr								program;
		FunctionPtr								func;
		map<string, vector<FunctionPtr>>		gVarFuncUse;
		map<string, vector<VariablePtr>>		gVarVarUse;
		string									gVar;
	protected:
		// global variable declaration
		virtual void preVisitGlobalLet(const VariablePtr & var) override {
			Visitor::preVisitGlobalLet(var);
			gVar = var->getMangledName();
			// var->used = false;
		}
		virtual VariablePtr visitGlobalLet(const VariablePtr & var) override {
			gVar.clear();
			return Visitor::visitGlobalLet(var);
		}
		// function
		virtual void preVisit(Function * f) override {
			Visitor::preVisit(f);
			func = f->shared_from_this();
			func->useFunctions.clear();
			func->useGlobalVariables.clear();
		}
		virtual FunctionPtr visit(Function * that) override {
			func.reset();
			return Visitor::visit(that);
		}
		// variable
		virtual void preVisit(ExprVar * expr) override {
			Visitor::preVisit(expr);
			if (!expr->local && !expr->argument && !expr->block) {
				if (func) {
					func->useGlobalVariables.insert(expr->variable);
				} else {
					gVarVarUse[gVar].push_back(expr->variable);
				}
			}
		}
		// function call
		virtual void preVisit(ExprCall * call) override {
			Visitor::preVisit(call);
			if (!call->func->builtIn) {
				if (func) {
					func->useFunctions.insert(call->func);
				} else {
					gVarFuncUse[gVar].push_back(call->func);
				}
			}
		}
		// Op1
		virtual void preVisit(ExprOp1 * expr) override {
			Visitor::preVisit(expr);
			if (!expr->func->builtIn) {
				if (func) {
					func->useFunctions.insert(expr->func);
				} else {
					gVarFuncUse[gVar].push_back(expr->func);
				}
			}
		}
		// Op2
		virtual void preVisit(ExprOp2 * expr) override {
			Visitor::preVisit(expr);
			if (!expr->func->builtIn) {
				if (func) {
					func->useFunctions.insert(expr->func);
				} else {
					gVarFuncUse[gVar].push_back(expr->func);
				}
			}
		}
		// Op3
		virtual void preVisit(ExprOp3 * expr) override {
			Visitor::preVisit(expr);
			if (expr->func && !expr->func->builtIn) {
				if (func) {
					func->useFunctions.insert(expr->func);
				} else {
					gVarFuncUse[gVar].push_back(expr->func);
				}
			}
		}
	};

	void Program::markOrRemoveUnusedSymbols() {
		MarkSymbolUse vis;
		visit(vis);
		vis.markUsedFunctions(*thisModule);
        if ( options.getOption("removeUnusedSymbols",true) ) {
            vis.RemoveUnusedSymbols(*thisModule);
        }
	}
}
