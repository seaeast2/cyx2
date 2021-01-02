#ifndef CVM_EXPR_HPP
#define CVM_EXPR_HPP

#include "ast_visitor.h"

namespace COMPILER
{
    class Expr : public AST
    {
      public:
        using AST::AST;
    };

    class IntExpr : public Expr
    {
      public:
        using Expr::Expr;
        int value;

        void visit(ASTVisitor *visitor) override
        {
            visitor->visitIntExpr(this);
        };
    };

    class DoubleExpr : public Expr
    {
      public:
        using Expr::Expr;
        double value;
        void visit(ASTVisitor *visitor) override
        {
            visitor->visitDoubleExpr(this);
        };
    };

    class IdentifierExpr : public Expr
    {
      public:
        using Expr ::Expr;
        std::string value;
        void visit(ASTVisitor *visitor) override
        {
            visitor->visitIdentifierExpr(this);
        };
    };

    class StringExpr : public Expr
    {
      public:
        using Expr::Expr;
        std::string value;
        void visit(ASTVisitor *visitor) override
        {
            visitor->visitStringExpr(this);
        };
    };

    class BinaryExpr : public Expr
    {
      public:
        using Expr::Expr;
        Expr *lhs{ nullptr };
        Token op;
        Expr *rhs{ nullptr };
        void visit(ASTVisitor *visitor) override
        {
            visitor->visitBinaryExpr(this);
        };
    };

    class UnaryExpr : public Expr
    {
      public:
        using Expr::Expr;
        Token op;
        Expr *rhs{ nullptr };
        void visit(ASTVisitor *visitor) override
        {
            visitor->visitUnaryExpr(this);
        };
    };

    class AssignExpr : public BinaryExpr
    {
      public:
        using BinaryExpr::BinaryExpr;
        void visit(ASTVisitor *visitor) override
        {
            visitor->visitAssignExpr(this);
        };
    };

    class FuncCallExpr : public Expr
    {
      public:
        using Expr::Expr;
        std::string func_name;
        std::vector<Stmt *> args;
        void visit(ASTVisitor *visitor) override
        {
            visitor->visitFuncCallExpr(this);
        };
    };
} // namespace COMPILER

#endif // CVM_EXPR_HPP
