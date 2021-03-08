#include "ir_generator.h"

#define LINK(PRE, SUCC)                                                                                                \
    PRE->addSucc(SUCC);                                                                                                \
    SUCC->addPre(PRE)

void COMPILER::IRGenerator::visitUnaryExpr(COMPILER::UnaryExpr *ptr)
{
    auto *assign = new IRAssign;
    auto *binary = new IRBinary;

    assign->block       = cur_basic_block;
    binary->belong_inst = assign;

    check_var_exist = true;
    ptr->rhs->visit(this);
    check_var_exist = false;

    if (inOr(ptr->op.keyword, SELFADD_PREFIX, SELFSUB_PREFIX, SELFADD_SUFFIX, SELFSUB_SUFFIX))
    {
        auto *self      = consumeVariable();
        auto *constant  = new IRConstant;
        constant->value = 1;

        assign->dest   = self;
        binary->lhs    = self;
        binary->opcode = token2IROp(ptr->op.keyword);
        binary->rhs    = constant;
        assign->dest   = self;
        assign->src    = binary;

        if (inOr(ptr->op.keyword, SELFSUB_SUFFIX, SELFADD_SUFFIX))
        {
            auto *assign2        = new IRAssign;
            auto *binary2        = new IRBinary;
            assign2->block       = cur_basic_block;
            binary2->belong_inst = assign2;

            binary2->lhs    = self;
            binary2->opcode = token2IROp(ptr->op.keyword);
            assign2->dest   = newVariable();
            assign2->src    = binary2;
            cur_basic_block->addInst(assign2);
        }
    }
    else
    {
        binary->opcode = token2IROp(ptr->op.keyword);
        binary->rhs    = consumeVariable();

        assign->dest = newVariable();
        assign->src  = binary;
    }
    cur_basic_block->addInst(assign);
}

void COMPILER::IRGenerator::visitBinaryExpr(COMPILER::BinaryExpr *ptr)
{
    auto *binary        = new IRBinary;
    auto *assign        = new IRAssign;
    binary->opcode      = token2IROp(ptr->op.keyword);
    binary->belong_inst = assign;
    assign->src         = binary;
    assign->block       = cur_basic_block;
    // lhs
    check_var_exist = true;
    ptr->lhs->visit(this);
    check_var_exist = false;
    if (cur_value.hasValue())
    {
        // int / double / string
        auto *lhs        = new IRConstant;
        lhs->value       = cur_value;
        lhs->belong_inst = assign;
        binary->lhs      = lhs;
        cur_value.reset();
    }
    else
    {
        auto *lhs        = consumeVariable();
        lhs->belong_inst = assign;
        binary->lhs      = lhs;
    }
    // rhs
    check_var_exist = true;
    ptr->rhs->visit(this);
    check_var_exist = false;
    if (cur_value.hasValue())
    {
        auto *rhs        = new IRConstant;
        rhs->value       = cur_value;
        rhs->belong_inst = assign;
        binary->rhs      = rhs;
        cur_value.reset();
    }
    else
    {
        auto *rhs        = consumeVariable();
        rhs->belong_inst = assign;
        binary->rhs      = rhs;
    }

    // constant folding
    int operand_count = 0;
    if (CONSTANT_FOLDING && inOr(ptr->op.keyword, ADD, SUB, MUL, DIV, BOR, BXOR, MOD, SHR, SHL))
    {
        CYX::Value operand1;
        CYX::Value operand2;
        CYX::Value value;

        auto *operand1_const_ptr = as<IRConstant, IR::Tag::CONST>(binary->lhs);
        auto *operand1_var_ptr   = as<IRVar, IR::Tag::VAR>(binary->lhs);
        auto *operand2_const_ptr = as<IRConstant, IR::Tag::CONST>(binary->rhs);
        auto *operand2_var_ptr   = as<IRVar, IR::Tag::VAR>(binary->rhs);

        if (operand1_const_ptr != nullptr)
        {
            operand1 = operand1_const_ptr->value;
            operand_count++;
        }
        if (operand2_const_ptr != nullptr)
        {
            operand2 = operand2_const_ptr->value;
            operand_count++;
        }
        if (operand1_var_ptr != nullptr)
        {
            auto *tmp_assign = as<IRAssign, IR::Tag::ASSIGN>(operand1_var_ptr->def->belong_inst);
            if (tmp_assign != nullptr && tmp_assign->src->tag == IR::Tag::CONST)
            {
                operand1 = as<IRConstant, IR::Tag::CONST>(tmp_assign->src)->value;
                operand_count++;
            }
        }
        if (operand2_var_ptr != nullptr)
        {
            auto *tmp_assign = as<IRAssign, IR::Tag::ASSIGN>(operand2_var_ptr->def->belong_inst);
            if (tmp_assign != nullptr && tmp_assign->src->tag == IR::Tag::CONST)
            {
                operand2 = as<IRConstant, IR::Tag::CONST>(tmp_assign->src)->value;
                operand_count++;
            }
        }
        if (operand_count == 2)
        {
            switch (ptr->op.keyword)
            {
                case ADD: value = operand1 + operand2; break;
                case SUB: value = operand1 - operand2; break;
                case MUL: value = operand1 * operand2; break;
                case DIV: value = operand1 / operand2; break;
                case BOR: value = operand1 | operand2; break;
                case BXOR: value = operand1 ^ operand2; break;
                case MOD: value = operand1 % operand2; break;
                case SHR: value = operand1 >> operand2; break;
                case SHL: value = operand1 << operand2; break;
                default: UNREACHABLE();
            }
            cur_value = value;
            // free memory
            destroyVar(operand1_var_ptr);
            destroyVar(operand2_var_ptr);
            delete operand1_const_ptr;
            delete operand2_const_ptr;
            delete binary;
        }
    }

    if (operand_count != 2)
    {
        assign->dest              = newVariable();
        assign->dest->belong_inst = assign;

        cur_basic_block->addInst(assign);
    }
}

void COMPILER::IRGenerator::visitIntExpr(COMPILER::IntExpr *ptr)
{
    cur_value = ptr->value;
}

void COMPILER::IRGenerator::visitDoubleExpr(COMPILER::DoubleExpr *ptr)
{
    cur_value = ptr->value;
}

void COMPILER::IRGenerator::visitStringExpr(COMPILER::StringExpr *ptr)
{
    cur_value = ptr->value;
}

void COMPILER::IRGenerator::visitAssignExpr(COMPILER::AssignExpr *ptr)
{
    if (step == 1)
    {
        auto *ir_var = new HIRVar;
        ir_var->name = static_cast<IdentifierExpr *>(ptr->lhs)->value;
        ir_var->rhs  = ptr->rhs;

        if (first_scan_funcs.find(ir_var->name) != first_scan_funcs.end())
        {
            ERROR("twice defined! previous " + ir_var->name + " defined is function!!");
        }

        first_scan_vars[ir_var->name] = ir_var;
        return;
    }
    // actually assign is one of the binary expr
    auto *assign  = new IRAssign;
    assign->block = cur_basic_block;
    //
    ptr->lhs->visit(this);
    auto *dest        = consumeVariable(false);
    dest->belong_inst = assign;
    assign->dest      = dest;
    //
    check_var_exist = true;
    ptr->rhs->visit(this);
    check_var_exist = false;
    if (cur_value.hasValue())
    {
        auto *constant        = new IRConstant;
        constant->value       = cur_value;
        constant->belong_inst = assign;
        assign->src           = constant;

        cur_value.reset();
    }
    else
    {
        auto *src        = consumeVariable();
        src->belong_inst = assign;
        assign->src      = src;
    }
    cur_basic_block->addInst(assign);
}

void COMPILER::IRGenerator::visitIdentifierExpr(COMPILER::IdentifierExpr *ptr)
{
    auto upval = cur_symbol->query(ptr->value);
    if (upval.type == Symbol::Type::VAR)
    {
        auto *var = new IRVar;
        var->name = ptr->value;
        var->def  = upval.var;
        upval.var->addUse(var);

        tmp_vars.push(var);
    }
    else if (upval.type == Symbol::Type::INVALID)
    {
        if (check_var_exist)
        {
            ERROR("cant find definition of " + ptr->value);
        }
        auto *var_def = new IRVar;
        var_def->name = ptr->value;

        Symbol symbol;
        symbol.type = Symbol::Type::VAR;
        symbol.var  = var_def;
        cur_symbol->upsert(var_def->name, symbol);
        tmp_vars.push(var_def);
        //
        check_var_exist = false;
    }
}

void COMPILER::IRGenerator::visitFuncCallExpr(COMPILER::FuncCallExpr *ptr)
{
    auto *inst = new IRCall;
    inst->name = ptr->func_name;
    inst->func = first_scan_funcs[ptr->func_name]->ir_func;

    int arg_cnt = 0;
    std::vector<IR *> tmp_arg;

    for (auto *arg : ptr->args)
    {
        arg->visit(this);
        if (cur_value.hasValue())
        {
            auto *constant  = new IRConstant;
            constant->value = cur_value;
            cur_value.reset();
            tmp_arg.push_back(constant);
            arg_cnt++;
        }
        else
        {
            tmp_arg.push_back(consumeVariable());
            arg_cnt++;
        }
    }
    // args
    for (; arg_cnt < ptr->args.size(); arg_cnt++)
    {
        inst->args.push_back(cur_basic_block->insts.back());
        cur_basic_block->insts.pop_back();
    }
    for (auto *arg : tmp_arg)
    {
        inst->args.push_back(arg);
    }

    inst->block   = cur_basic_block;
    auto *assign  = new IRAssign;
    assign->dest  = newVariable();
    assign->block = cur_basic_block;
    assign->src   = inst;
    cur_basic_block->insts.push_back(assign);
}

void COMPILER::IRGenerator::visitExprStmt(COMPILER::ExprStmt *ptr)
{
    ptr->expr->visit(this);
}

void COMPILER::IRGenerator::visitIfStmt(COMPILER::IfStmt *ptr)
{
    auto *cond_block = newBasicBlock();
    LINK(cur_basic_block, cond_block);
    cur_basic_block = cond_block;
    ptr->cond->visit(this);
    //
    auto *true_block = newBasicBlock();
    LINK(cond_block, true_block);
    cur_basic_block = true_block;
    ptr->true_block->visit(this);
    auto *out_of_true = cur_basic_block;
    //
    auto *false_block = newBasicBlock();
    LINK(cond_block, false_block);
    cur_basic_block = false_block;
    if (ptr->false_block != nullptr) ptr->false_block->visit(this);
    auto *out_of_false = cur_basic_block;
    auto *out_block    = newBasicBlock();
    LINK(out_of_true, out_block);
    LINK(out_of_false, out_block);
    //
    cur_basic_block = out_block;
    //
    auto *branch        = new IRBranch;
    branch->true_block  = true_block;
    branch->false_block = false_block;
    branch->block       = cond_block;
    branch->cond        = consumeVariable();
    cond_block->addInst(branch);
}

void COMPILER::IRGenerator::visitForStmt(COMPILER::ForStmt *ptr)
{
    /*
    IR vm_insts similar to the following vm_insts.
    //
    @init
     t0=1
    @cond
     t2=t0<3
     if t2
     goto @body
     goto @out
    @final
     t0=t0+1
     goto @cond
    @body
     t1=t1+1
     goto @final
    @out
     ....
    */
    auto *init_block = newBasicBlock();
    //
    loop_stack.push_back(init_block);
    //
    LINK(cur_basic_block, init_block);
    cur_basic_block = init_block;
    ptr->init->visit(this);
    //
    auto *cond_block = newBasicBlock();
    LINK(cur_basic_block, cond_block);
    cur_basic_block = cond_block;
    ptr->cond->visit(this);

    auto *branch      = new IRBranch;
    auto *cond        = consumeVariable();
    cond->belong_inst = branch;
    branch->cond      = cond;
    cur_basic_block->addInst(branch);
    //
    auto *body_block = newBasicBlock();
    LINK(cur_basic_block, body_block);
    cur_basic_block = body_block;
    ptr->block->visit(this);
    //
    auto *final_block = newBasicBlock();
    LINK(cur_basic_block, final_block);
    LINK(final_block, cond_block);
    cur_basic_block = final_block;
    ptr->final->visit(this);

    branch->true_block = body_block;
    auto *jmp          = new IRJump;
    cur_basic_block->addInst(jmp);

    jmp->target = cond_block;
    //
    auto *out_block = newBasicBlock();
    LINK(cond_block, out_block);
    cur_basic_block     = out_block;
    branch->false_block = out_block;
    // loop
    out_block->loop_start = init_block;
    init_block->loop_end  = out_block;
    //
    loop_stack.pop_back();
}

void COMPILER::IRGenerator::visitWhileStmt(COMPILER::WhileStmt *ptr)
{
    auto *cond_block = newBasicBlock();
    //
    loop_stack.push_back(cond_block);
    //
    LINK(cur_basic_block, cond_block);
    cur_basic_block = cond_block;
    ptr->cond->visit(this);
    auto *branch  = new IRBranch;
    branch->block = cur_basic_block;
    branch->cond  = consumeVariable();

    auto *body_block = newBasicBlock();
    LINK(cond_block, body_block);
    cur_basic_block = body_block;
    ptr->block->visit(this);
    //
    auto *jmp   = new IRJump;
    jmp->target = cond_block;
    cur_basic_block->addInst(jmp);
    LINK(cur_basic_block, cond_block);

    auto *out_block = newBasicBlock();
    LINK(cond_block, out_block);

    branch->true_block  = body_block;
    branch->false_block = out_block;
    cond_block->addInst(branch);

    cur_basic_block = out_block;
    // loop
    cond_block->loop_end  = out_block;
    out_block->loop_start = cond_block;
    //
    loop_stack.pop_back();
}

void COMPILER::IRGenerator::visitSwitchStmt(COMPILER::SwitchStmt *ptr)
{
}

void COMPILER::IRGenerator::visitMatchStmt(COMPILER::MatchStmt *ptr)
{
}

void COMPILER::IRGenerator::visitFuncDeclStmt(COMPILER::FuncDeclStmt *ptr)
{
    // first scan...
    if (step != 1) return;

    auto *func    = new HIRFunction;
    func->ir_func = new IRFunction;
    func->name    = ptr->func_name->value;
    func->block   = ptr->block;
    for (const auto &param : ptr->params)
    {
        auto *var = new IRVar;
        var->name = param;
        func->params.push_back(var);
    }

    if (first_scan_vars.find(func->name) != first_scan_vars.end())
    {
        ERROR("twice defined! previous " + func->name + " defined is variable!!");
    }
    first_scan_funcs[func->name] = func;
}

void COMPILER::IRGenerator::visitBreakStmt(COMPILER::BreakStmt *ptr)
{
    if (loop_stack.empty()) ERROR("unexpected BreakStmt");
    auto *inst   = new IRJump;
    inst->target = loop_stack.back();
    fix_continue_wait_list.push_back(inst);
    cur_basic_block->addInst(inst);
}

void COMPILER::IRGenerator::visitContinueStmt(COMPILER::ContinueStmt *ptr)
{
    if (loop_stack.empty()) ERROR("unexpected ContinueStmt");
    auto *inst   = new IRJump;
    inst->target = loop_stack.back();
    cur_basic_block->addInst(inst);
}

void COMPILER::IRGenerator::visitReturnStmt(COMPILER::ReturnStmt *ptr)
{
    ptr->retval->visit(this);
    auto *inst = new IRReturn;
    if (cur_value.hasValue())
    {
        auto *constant  = new IRConstant;
        constant->value = cur_value;
        cur_value.reset();
        inst->ret = constant;
    }
    else if (ptr->retval != nullptr)
    {
        auto *var = consumeVariable();
        inst->ret = var;
    }
    else
    {
        // return void
    }
    cur_basic_block->addInst(inst);
}

void COMPILER::IRGenerator::visitImportStmt(COMPILER::ImportStmt *ptr)
{
}

void COMPILER::IRGenerator::visitBlockStmt(COMPILER::BlockStmt *ptr)
{
    enterNewScope();

    for (auto &x : ptr->stmts)
    {
        x->visit(this);
    }

    exitScope();
}

void COMPILER::IRGenerator::enterNewScope()
{
    cur_symbol = new SymbolTable(cur_symbol);
}

void COMPILER::IRGenerator::exitScope()
{
    auto *tmp  = cur_symbol;
    cur_symbol = cur_symbol->pre;
    delete tmp;
}

COMPILER::IRVar *COMPILER::IRGenerator::newVariable()
{
    Symbol symbol;
    symbol.type = Symbol::Type::VAR;

    auto *ir_var      = new IRVar;
    ir_var->name      = "t" + std::to_string(var_cnt++);
    ir_var->is_ir_gen = true;

    symbol.var = ir_var;

    cur_symbol->upsert(ir_var->name, symbol);
    tmp_vars.push(ir_var);
    return tmp_vars.top();
}

std::string COMPILER::IRGenerator::newLabel()
{
    return "L" + std::to_string(label_cnt++);
}

COMPILER::IRVar *COMPILER::IRGenerator::consumeVariable(bool force_IRVar)
{
    if (!force_IRVar)
    {
        auto *retval = tmp_vars.top();
        tmp_vars.pop();
        return retval;
    }

    auto *var_def = tmp_vars.top(); // variable definition
    tmp_vars.pop();

    auto *retval      = new IRVar;
    retval->name      = var_def->name;
    retval->is_ir_gen = var_def->is_ir_gen;

    // def-use.
    if (var_def->def == nullptr)
    {
        var_def->addUse(retval);
        retval->def = var_def;
    }
    else
    {
        retval->def = var_def->def;
        var_def->def->addUse(retval);
    }

    return retval;
}

COMPILER::IRGenerator::IRGenerator()
{
    cur_symbol = new SymbolTable(global_table);
}

std::string COMPILER::IRGenerator::irCodeString()
{
    std::string ir_code = "There are " + std::to_string(global_var_decl->insts.size()) + " variable(s) declared!\n";
    for (auto *var : global_var_decl->insts)
    {
        addSpace(ir_code, 2);
        ir_code += var->toString() + "\n";
    }
    ir_code += "\n";

    ir_code += "There are " + std::to_string(funcs.size()) + " function(s) declared!\n";
    for (const auto *func : funcs)
    {
        ir_code += "@FUNC_" + func->name + " ";
        ir_code += std::to_string(func->params.size()) + " param(s) -> (";
        for (const auto *param : func->params)
        {
            ir_code += param->name + ", ";
        }
        ir_code += ")\n";
        for (auto *block : func->blocks)
        {
            addSpace(ir_code, 2);
            ir_code += "@" + block->name;
            ir_code += " " + std::to_string(block->phis.size()) + " phis and " + std::to_string(block->insts.size()) +
                       " inst(s)\n";

            for (auto *phi : block->phis)
            {
                addSpace(ir_code, 4);
                ir_code += phi->toString() + "\n";
            }

            for (auto *inst : block->insts)
            {
                addSpace(ir_code, 4);
                ir_code += inst->toString() + "\n";
            }
        }
        ir_code += "\n";
    }
    return ir_code;
}

void COMPILER::IRGenerator::visitTree(COMPILER::Tree *ptr)
{
    if (cur_basic_block == nullptr) cur_basic_block = newBasicBlock();
    for (auto *x : ptr->stmts)
    {
        x->visit(this);
    }
    step            = 0;
    global_var_decl = new BasicBlock();
    for (const auto &x : first_scan_vars)
    {
        Symbol symbol;
        symbol.type = Symbol::Type::VAR;

        x.second->rhs->visit(this);
        auto *var_def = new IRVar;
        var_def->name = x.first;

        auto *assign  = new IRAssign;
        assign->block = cur_basic_block;
        assign->dest  = var_def;

        if (cur_value.hasValue())
        {
            auto *constant  = new IRConstant;
            constant->value = cur_value;
            cur_value.reset();
            assign->src = constant;

            symbol.var = var_def;
        }
        else
        {
            symbol.var  = consumeVariable(false);
            assign->src = symbol.var;
        }
        cur_symbol->upsert(x.first, symbol);
        global_var_decl->addInst(assign);
    }
    for (const auto &x : first_scan_funcs)
    {
        loop_stack.clear();

        auto *func = x.second->ir_func;
        func->name = x.first;
        Symbol symbol;
        symbol.type = Symbol::Type::FUNC;
        symbol.func = func;
        cur_symbol->upsert(func->name, symbol);

        cur_func = func;
        funcs.push_back(func);

        cur_basic_block = newBasicBlock();

        enterNewScope();
        for (auto param : x.second->params)
        {
            Symbol param_symbol_table;
            param_symbol_table.type = Symbol::Type::VAR;
            param_symbol_table.var  = param;
            cur_symbol->upsert(param->name, param_symbol_table);

            func->params.push_back(param);
        }

        x.second->block->visit(this);
        exitScope();
        // fix continue stmt, when visit continue stmt, we cant known the out block of the loop
        fixContinueTarget();
    }
}

COMPILER::IRGenerator::~IRGenerator()
{
    // TODO
}
COMPILER::BasicBlock *COMPILER::IRGenerator::newBasicBlock(const std::string &name)
{
    auto *bb = name.empty() ? new BasicBlock(newLabel()) : new BasicBlock(name);
    if (cur_func != nullptr)
    {
        cur_func->blocks.push_back(bb);
    }
    return bb;
}

void COMPILER::IRGenerator::simplifyIR()
{
    /* remove redundant ir like
     * t0 = 1 + 2
     * t1 = t0
     */
    for (auto *func : funcs)
    {
        for (auto *block : func->blocks)
        {
            if (block->insts.size() < 2) continue;
            for (auto it = block->insts.begin(); it != block->insts.end();)
            {
                auto *tmp_cur  = *it;
                auto *tmp_next = ++it != block->insts.end() ? *it : nullptr; // next inst iterator
                if (tmp_next == nullptr) break;
                // two insts must be IRAssign
                // MAGIC
                if (tmp_cur->tag == IR::Tag::ASSIGN && tmp_next->tag == IR::Tag::ASSIGN)
                {
                    auto *cur  = static_cast<IRAssign *>(tmp_cur);
                    auto *next = static_cast<IRAssign *>(tmp_next);
                    if (next->src->tag == IR::Tag::VAR && cur->dest->def == nullptr)
                    {
                        auto *var = static_cast<IRVar *>(next->src);
                        if (static_cast<IRVar *>(cur->dest)->is_ir_gen && var->name == cur->dest->name)
                        {
                            next->src = cur->src;
                            it        = block->insts.erase(--it); // it is pointing to `next` before --it.
                        }
                    }
                }
            }
        }
    }
}

void COMPILER::IRGenerator::removeUnusedVarDef()
{
    // TODO: `branch` inst may use val
    // TODO: fixContinueTarget() may cause some errors(i guess)
    for (auto *func : funcs)
    {
        // reverse traversal
        for (auto block_it = func->blocks.crbegin(); block_it != func->blocks.crend(); block_it++)
        {
            auto *block = *block_it;
            for (auto inst_it = block->insts.rbegin(); inst_it != block->insts.rend();)
            {
                auto *inst = *inst_it;
                if (inst->tag != IR::Tag::ASSIGN) return;
                auto *assign = static_cast<IRAssign *>(inst);
                if (assign->dest->def == nullptr && assign->dest->use.empty())
                {
                    block->insts.erase(--inst_it.base());
                    if (assign->src->tag == IR::Tag::BINARY)
                    {
                        auto *binary = static_cast<IRBinary *>(assign->src);
                        auto *lhs    = binary->lhs;
                        auto *rhs    = binary->rhs;
                        if (lhs->tag == IR::Tag::VAR)
                        {
                            auto *tmp = static_cast<IRVar *>(lhs);
                            if (tmp->def != nullptr) tmp->def->killUse(tmp);
                        }
                        if (rhs->tag == IR::Tag::VAR)
                        {
                            auto *tmp = static_cast<IRVar *>(rhs);
                            if (tmp->def != nullptr) tmp->def->killUse(tmp);
                        }

                        delete lhs;
                        delete rhs;
                        delete binary;
                    }
                    else if (assign->src->tag == IR::Tag::CONST)
                    {
                        delete assign->src;
                    }
                    delete assign;
                }
                else
                {
                    inst_it--;
                }
            }
        }
    }
}

void COMPILER::IRGenerator::fixContinueTarget()
{
    for (auto *inst : fix_continue_wait_list)
    {
        auto *candidate_block = inst->target->loop_end;
        // if loop out block is empty block, it will be removed at CFG.simplifyCFG(), so we need find a not empty
        // succ block.
        while (candidate_block->insts.empty())
        {
            candidate_block = *candidate_block->succs.begin();
            if (!candidate_block->insts.empty())
            {
                break;
            }
            else if (candidate_block->succs.size() > 1)
                ERROR("loop out block has two or more succs, it impossible!");
        }
        inst->target = candidate_block;
    }
}

void COMPILER::IRGenerator::destroyVar(IRVar *var)
{
    if (var == nullptr) return;
    // kill define
    if (var->def)
    {
        var->def->killUse(var);
    }
    if (!var->use.empty())
    {
        ERROR("this var is some vars' defined");
    }
    delete var;
}

#undef LINK