#ifndef CVM_IR_GENERATOR_H
#define CVM_IR_GENERATOR_H

#include "../../common/buildin.hpp"
#include "../../common/config.h"
#include "../../utility/utility.hpp"
#include "../ast/ast_visitor.h"
#include "../ast/expr.hpp"
#include "../ast/stmt.hpp"
#include "../symbol.hpp"
#include "ir_instruction.hpp"

#include <stack>

namespace COMPILER
{
    class IRGenerator : public ASTVisitor
    {
      public:
        IRGenerator();
        ~IRGenerator();
        void visitTree(Tree *ptr) override;
        std::string irStr();
        //
        void removeUnusedVarDef();
        void simplifyIR();

      private:
        //
        void visitUnaryExpr(UnaryExpr *ptr) override;
        void visitBinaryExpr(BinaryExpr *ptr) override;
        void visitIntExpr(IntExpr *ptr) override;
        void visitDoubleExpr(DoubleExpr *ptr) override;
        void visitStringExpr(StringExpr *ptr) override;
        void visitAssignExpr(AssignExpr *ptr) override;
        void visitIdentifierExpr(IdentifierExpr *ptr) override;
        void visitFuncCallExpr(FuncCallExpr *ptr) override;
        void visitArrayExpr(ArrayExpr *ptr) override;
        void visitArrayIdExpr(ArrayIdExpr *ptr) override;

      private:
        //
        void visitExprStmt(ExprStmt *ptr) override;
        void visitIfStmt(IfStmt *ptr) override;
        void visitForStmt(ForStmt *ptr) override;
        void visitWhileStmt(WhileStmt *ptr) override;
        void visitMatchStmt(MatchStmt *ptr) override;
        void visitSwitchStmt(SwitchStmt *ptr) override;
        void visitFuncDeclStmt(FuncDeclStmt *ptr) override;
        void visitBreakStmt(BreakStmt *ptr) override;
        void visitContinueStmt(ContinueStmt *ptr) override;
        void visitReturnStmt(ReturnStmt *ptr) override;
        void visitImportStmt(ImportStmt *ptr) override;
        void visitBlockStmt(BlockStmt *ptr) override;
        //
        void enterNewScope();
        void exitScope();
        //
        IRVar *newVariable();
        std::string newLabel();
        // variable 을 stack 에서 꺼내서 IRVar 로 포장해서 반환
        COMPILER::IRVar *consumeVariable(bool force_IRVar = true);
        BasicBlock *newBasicBlock(const std::string &name = "");
        void destroyVar(IRVar *var);
        //
        void fixEdges();

      private:
        std::vector<BasicBlock *> loop_stack;

        std::stack<std::vector<IRJump *> *> fix_break_wait_list_stack;
        std::stack<std::vector<IRJump *> *> fix_continue_wait_list_stack;
        std::vector<IRJump *> *cur_fix_break_wait_list{ nullptr };
        std::vector<IRJump *> *cur_fix_continue_wait_list{ nullptr };

      private:
        int var_cnt{ 0 };
        int label_cnt{ 0 };
        SymbolTable global_table;
        //
        bool check_var_exist{ false }; // 딱히 별 역할을 하지는 않음. 뭔가 하려고 만들었다가 안한듯.
        // 하위식에서 나온 변수를 상위식에서 사용하기 위해서 임시로 저장하는 곳
        std::stack<IRVar *> tmp_vars;
        // cur_value 는 literal 에서 읽어온 값을 저장한다.
        CYX::Value cur_value; // Value 는 모든 값들을 담아 놓을 수 있는 union container 역할
        IRFunction *cur_func{ nullptr };
        BasicBlock *cur_basic_block{ nullptr };
        SymbolTable *cur_symbol{ nullptr };

      public:
        std::vector<IRFunction *> funcs;
        BasicBlock *global_var_decl{ nullptr };

      private:
        // AST first scan.
        class HIRFunction
        {
          public:
            std::string name;
            IRFunction *ir_func{ nullptr };
            std::vector<IRVar *> params;
            BlockStmt *block{ nullptr }; // ast body
            std::string toString() const
            {
                return name + "#" + std::to_string(params.size());
            }
        };

        class HIRVar
        {
          public:
            std::string name;
            Expr *rhs{ nullptr };
        };

        int step = 1;
        std::unordered_map<std::string, HIRFunction *> first_scan_funcs;
        std::unordered_map<std::string, HIRVar *> first_scan_vars;
    };

} // namespace COMPILER

#endif // CVM_IR_GENERATOR_H
