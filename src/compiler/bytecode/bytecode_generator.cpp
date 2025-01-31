#include "bytecode_generator.h"

COMPILER::BytecodeGenerator::BytecodeGenerator()
{
    // add buildin functions to table
    for (const auto &x : buildin_functions)
    {
        funcs_table[x.first.substr(8)] = 0 - x.second.second;
    }
}

void COMPILER::BytecodeGenerator::addInst(CVM::VMInstruction *inst)
{
    bytecode_basicblocks.back()->vm_insts.push_back(inst);
}

void COMPILER::BytecodeGenerator::ir2VmInst()
{
    bytecode_basicblocks.push_back(new BytecodeBasicBlock("global_var_decl"));
    genGlobalVarDecl();
    
    global_var_len = bytecode_basicblocks.back()->vm_insts.size();
    for (auto *func : funcs)
    {
        bytecode_basicblocks.push_back(new BytecodeBasicBlock(func->name));
        block_table.clear();
        genFunc(func);
        for (auto *block : func->blocks)
        {
            bytecode_basicblocks.push_back(new BytecodeBasicBlock(block->name));
            for (auto *inst : block->insts)
            {
                if (auto *ptr = as<IRAssign, IR::Tag::ASSIGN>(inst); ptr != nullptr)
                {
                    genAssign(ptr);
                }
                else if (auto *ptr = as<IRBinary, IR::Tag::BINARY>(inst); ptr != nullptr)
                {
                    genBinary(ptr);
                }
                else if (auto *ptr = as<IRReturn, IR::Tag::RETURN>(inst); ptr != nullptr)
                {
                    genReturn(ptr);
                }
                else if (auto *ptr = as<IRJump, IR::Tag::JMP>(inst); ptr != nullptr)
                {
                    genJump(ptr);
                }
                else if (auto *ptr = as<IRCall, IR::Tag::CALL>(inst); ptr != nullptr)
                {
                    genCall(ptr);
                }
                else if (auto *ptr = as<IRBranch, IR::Tag::BRANCH>(inst); ptr != nullptr)
                {
                    genBranch(ptr);
                }
                else
                {
                    UNREACHABLE();
                }
            }
        }
        if (func->name == ENTRY_FUNC) entry_end_block_name = func->blocks.back()->name;
    }
}

void COMPILER::BytecodeGenerator::genBinary(COMPILER::IRBinary *ptr)
{
    // a = b + c
    // load b to register %?
    if (auto *lhs = as<IRVar, IR::Tag::VAR>(ptr->lhs); lhs != nullptr)
    {
        std::vector<CVM::ArrIdx> arr_idx;
        parseVarArr(lhs, arr_idx);
        genLoadX(1, lhs->ssaName(), arr_idx);
        // self add / sub
        if (ptr->rhs == nullptr) return;
    }
    else if (auto *lhs = as<IRConstant, IR::Tag::CONST>(ptr->lhs); lhs != nullptr)
    {
        genLoadConst(lhs->value, 1);
    }
    else
    {
        // there is noting to do.
        // unary expr will reach here
    }
    // load c to register %?
    if (auto *rhs = as<IRVar, IR::Tag::VAR>(ptr->rhs); rhs != nullptr)
    {
        if (ptr->lhs == nullptr) // unary expr, ~a
        {
            if (ptr->opcode == IROpcode::IR_BNOT)
            {
                std::vector<CVM::ArrIdx> arr_idx;
                parseVarArr(rhs, arr_idx);
                genLoadX(1, rhs->ssaName(), arr_idx);
                auto *inst    = new CVM::Bnot;
                inst->reg_idx = 1;
                inst->name    = rhs->ssaName();
                inst->type    = CVM::ArgType::MAP;
                addInst(inst);
                return;
            }
        }
        else
        {
            std::vector<CVM::ArrIdx> arr_idx;
            parseVarArr(rhs, arr_idx);
            genLoadX(2, rhs->ssaName(), arr_idx);
        }
    }
    else if (auto *rhs = as<IRConstant, IR::Tag::CONST>(ptr->rhs); rhs != nullptr)
    {
        genLoadConst(rhs->value, 2);
    }
    else
    {
        UNREACHABLE();
    }
    //
#define CASE_OPCODE(VM_OPCODE, IR_OPCODE)                                                                              \
    if (ptr->opcode == IROpcode::IR_##IR_OPCODE)                                                                       \
    {                                                                                                                  \
        auto *inst     = new CVM::VM_OPCODE;                                                                           \
        inst->reg_idx1 = 1;                                                                                            \
        inst->reg_idx2 = 2;                                                                                            \
        addInst(inst);                                                                                                 \
    }

#define ELSE_CASE_OPCODE(VM_OPCODE, IR_OPCODE) else CASE_OPCODE(VM_OPCODE, IR_OPCODE)

    CASE_OPCODE(Add, ADD)
    ELSE_CASE_OPCODE(Sub, SUB)
    ELSE_CASE_OPCODE(Mul, MUL)
    ELSE_CASE_OPCODE(Div, DIV)
    ELSE_CASE_OPCODE(Mod, MOD)
    ELSE_CASE_OPCODE(Band, BAND)
    ELSE_CASE_OPCODE(Bxor, BXOR)
    ELSE_CASE_OPCODE(Bor, BOR)
    ELSE_CASE_OPCODE(Bxor, BXOR)
    ELSE_CASE_OPCODE(Exp, EXP)
    ELSE_CASE_OPCODE(Shl, SHL)
    ELSE_CASE_OPCODE(Shr, SHR)
    // CMP
    ELSE_CASE_OPCODE(Land, LAND)
    ELSE_CASE_OPCODE(Lor, LOR)
    ELSE_CASE_OPCODE(Eq, EQ)
    ELSE_CASE_OPCODE(Ne, NE)
    ELSE_CASE_OPCODE(Le, LE)
    ELSE_CASE_OPCODE(Lt, LT)
    ELSE_CASE_OPCODE(Ge, GE)
    ELSE_CASE_OPCODE(Gt, GT)

#undef ELSE_CASE_OPCODE
#undef CASE_OPCODE
}

void COMPILER::BytecodeGenerator::genLoadConst(CYX::Value &val, int reg_idx)
{
    if (val.is<long long>())
        genLoad(reg_idx, val.as<long long>());
    else if (val.is<double>())
        genLoad(reg_idx, val.as<double>());
    else if (val.is<std::string>())
        genLoad(reg_idx, val.as<std::string>());
}

void COMPILER::BytecodeGenerator::genStoreConst(CYX::Value &val, const std::string &name)
{
    if (val.is<long long>())
        genStore(name, val.as<long long>());
    else if (val.is<double>())
        genStore(name, val.as<double>());
    else if (val.is<std::string>())
        genStore(name, val.as<std::string>());
    else
        UNREACHABLE();
}

void COMPILER::BytecodeGenerator::genReturn(COMPILER::IRReturn *ptr)
{
    auto *ret = new CVM::Ret;
    if (ptr->ret != nullptr)
    {
        auto *constant = as<IRConstant, IR::Tag::CONST>(ptr->ret);
        auto *var      = as<IRVar, IR::Tag::VAR>(ptr->ret);
        if (constant != nullptr)
        {
            if (constant->value.is<long long>())
                genLoad(1, constant->value.as<long long>());
            else if (constant->value.is<double>())
                genLoad(1, constant->value.as<double>());
            else if (constant->value.is<std::string>())
                genLoad(1, constant->value.as<std::string>());
        }
        else if (var != nullptr)
        {
            std::vector<CVM::ArrIdx> arr_idx;
            parseVarArr(var, arr_idx);
            genLoadX(1, var->ssaName(), arr_idx);
        }
        // if has more retval.....unsupported now.
        ret->ret_size = 1;
        ret->ret_regs.push_back(1);
    }
    addInst(ret);
}

void COMPILER::BytecodeGenerator::genJump(COMPILER::IRJump *ptr)
{
    auto *jmp             = new CVM::Jmp;
    jmp->basic_block_name = ptr->target->name;
    addInst(jmp);
}

void COMPILER::BytecodeGenerator::genJif(BasicBlock *target1, BasicBlock *target2)
{
    auto *jif              = new CVM::Jif;
    jif->basic_block_name1 = target1->name;
    jif->basic_block_name2 = target2->name;
    addInst(jif);
}

void COMPILER::BytecodeGenerator::genCall(COMPILER::IRCall *ptr)
{
    auto *call = new CVM::Call;
    call->name = ptr->name;
    addInst(call);
    for (auto *arg : ptr->args)
    {
        auto x = new CVM::Arg;
        if (auto var = as<IRVar, IR::Tag::VAR>(arg); var != nullptr)
        {
            std::vector<CVM::ArrIdx> arr_idx;
            parseVarArr(var, arr_idx);
            x->index = arr_idx;
            x->type  = CVM::ArgType::MAP;
            x->name  = var->ssaName();
        }
        else if (auto constant = as<IRConstant, IR::Tag::CONST>(arg); arg != nullptr)
        {
            x->type  = CVM::ArgType::RAW;
            x->value = constant->value;
        }
        addInst(x);
    }
}

void COMPILER::BytecodeGenerator::genFunc(COMPILER::IRFunction *ptr)
{
    auto *func_inst        = new CVM::Func();
    func_inst->name        = ptr->name;
    func_inst->param_count = ptr->params.size();
    addInst(func_inst);
    // Param
    for (auto param : ptr->params)
    {
        auto *p = new CVM::Param();
        p->name = param->ssaName();
        addInst(p);
    }
}

void COMPILER::BytecodeGenerator::genBranch(COMPILER::IRBranch *ptr)
{
    genLoadX(STATE_REGISTER, ptr->cond->ssaName()); // state register
    genJif(ptr->true_block, ptr->false_block);
}

void COMPILER::BytecodeGenerator::genAssign(COMPILER::IRAssign *ptr)
{
    /*
     * a = c + d
     * ->
     * load c 1
     * load d 2
     * add 1 2
     * storex a 1
     * */
    std::string lhs = ptr->dest()->ssaName();

    std::vector<CVM::ArrIdx> arr_idx;
    parseVarArr(ptr->dest(), arr_idx); // handle array index start
    if (auto *binary = as<IRBinary, IR::Tag::BINARY>(ptr->src()); binary != nullptr)
    {
        genBinary(binary);
        if (inOr(binary->opcode, IROpcode::IR_LE, IROpcode::IR_LT, IROpcode::IR_GE, IROpcode::IR_GT, IROpcode::IR_LAND,
                 IROpcode::IR_LOR, IROpcode::IR_EQ, IROpcode::IR_NE))
            genStoreX(lhs, 0, arr_idx);
        else
            genStoreX(lhs, 1, arr_idx);
    }
    else if (auto *constant = as<IRConstant, IR::Tag::CONST>(ptr->src()); constant != nullptr)
    {
        if (ptr->dest()->is_array)
            genStoreA(lhs, constant->value, arr_idx);
        else
            genStoreConst(constant->value, lhs);
    }
    else if (auto *var = as<IRVar, IR::Tag::VAR>(ptr->src()); var != nullptr)
    {
        std::vector<CVM::ArrIdx> src_idx;
        parseVarArr(var, src_idx);

        genLoadX(1, var->ssaName(), src_idx);
        genStoreX(lhs, 1, arr_idx);
    }
    else if (auto *arr = as<IRArray, IR::Tag::ARRAY>(ptr->src()); arr != nullptr)
    {
        std::vector<CYX::Value> value;
        std::vector<std::pair<std::string, int>> idx;
        /*
         * a = [3,4]
         * b = [1,a,2]
         * c = [a,b,a,b,a,b,a,b,a,b,a,2]
         * */
        for (int i = 0; i < arr->content.size(); i++)
        {
            if (auto *var = as<IRVar, IR::Tag::VAR>(arr->content[i]); var != nullptr)
            {
                value.emplace_back();
                idx.emplace_back(var->name, i);
            }
            else if (auto *constant = as<IRConstant, IR::Tag::CONST>(arr->content[i]); constant != nullptr)
            {
                value.push_back(constant->value);
            }
            else
            {
                UNREACHABLE();
            }
        }
        genLoadA(2, value);
        genLoadXA(2, idx);
        genStoreX(lhs, 2, arr_idx);
    }
    else if (auto *call = as<IRCall, IR::Tag::CALL>(ptr->src()); call != nullptr)
    {
        genCall(call);
        genStoreX(lhs, 1, arr_idx);
    }
    else
    {
        UNREACHABLE();
    }
}

template<typename T>
void COMPILER::BytecodeGenerator::genLoad(int reg_idx, T val)
{
    if constexpr (std::is_same<T, long long>())
    {
        auto *load_i    = new CVM::LoadI;
        load_i->val     = val;
        load_i->reg_idx = reg_idx;
        addInst(load_i);
    }
    else if constexpr (std::is_same<T, double>())
    {
        auto *load_d    = new CVM::LoadD;
        load_d->val     = val;
        load_d->reg_idx = reg_idx;
        addInst(load_d);
    }
    else if constexpr (std::is_same<T, std::string>())
    {
        auto *load_d    = new CVM::LoadS;
        load_d->val     = val;
        load_d->reg_idx = reg_idx;
        addInst(load_d);
    }
    else
    {
        UNREACHABLE();
    }
}

void COMPILER::BytecodeGenerator::genLoadX(int reg_idx, const std::string &name)
{
    auto *load_x    = new CVM::LoadX;
    load_x->name    = name;
    load_x->reg_idx = reg_idx;
    addInst(load_x);
}

void COMPILER::BytecodeGenerator::genLoadX(int reg_idx, const std::string &name, const std::vector<CVM::ArrIdx> &idx)
{
    genLoadX(reg_idx, name);
    if (idx.empty()) return;
    auto *inst  = static_cast<CVM::LoadX *>(bytecode_basicblocks.back()->vm_insts.back());
    inst->index = idx;
}

void COMPILER::BytecodeGenerator::genLoadXA(int reg_idx, const std::vector<std::pair<std::string, int>> &idx)
{
    for (const auto &x : idx)
    {
        auto *load_xa    = new CVM::LoadXA;
        load_xa->name    = x.first;
        load_xa->reg_idx = reg_idx;
        load_xa->index   = x.second;
        addInst(load_xa);
    }
}

void COMPILER::BytecodeGenerator::genLoadA(int reg_idx, const std::vector<CYX::Value> &index)
{
    auto *load_a    = new CVM::LoadA;
    load_a->reg_idx = reg_idx;
    load_a->array   = index;
    addInst(load_a);
}

void COMPILER::BytecodeGenerator::genStoreA(const std::string &name, CYX::Value &val,
                                            const std::vector<CVM::ArrIdx> &idx)
{
    auto *store_a  = new CVM::StoreA;
    store_a->name  = name;
    store_a->value = val;
    store_a->index = idx;
    addInst(store_a);
}

template<typename T>
void COMPILER::BytecodeGenerator::genStore(const std::string &name, T val)
{
    if constexpr (std::is_same<T, long long>())
    {
        auto *store_i = new CVM::StoreI;
        store_i->val  = val;
        store_i->name = name;
        addInst(store_i);
    }
    else if constexpr (std::is_same<T, double>())
    {
        auto *store_d = new CVM::StoreD;
        store_d->val  = val;
        store_d->name = name;
        addInst(store_d);
    }
    else if constexpr (std::is_same<T, std::string>())
    {
        auto *store_d = new CVM::StoreS;
        store_d->val  = val;
        store_d->name = name;
        addInst(store_d);
    }
    else
    {
        UNREACHABLE();
    }
}

void COMPILER::BytecodeGenerator::genStoreX(const std::string &name, int reg_idx)
{
    auto *store_x    = new CVM::StoreX;
    store_x->name    = name;
    store_x->reg_idx = reg_idx;
    addInst(store_x);
}

void COMPILER::BytecodeGenerator::genStoreX(const std::string &name, int reg_idx, const std::vector<CVM::ArrIdx> &idx)
{
    genStoreX(name, reg_idx);
    if (idx.empty()) return;
    auto *inst  = static_cast<CVM::StoreX *>(bytecode_basicblocks.back()->vm_insts.back());
    inst->index = idx;
}

void COMPILER::BytecodeGenerator::collectBlockMap()
{
    int cnt = bytecode_basicblocks[0]->vm_insts.size();
    for (int i = 1; i < bytecode_basicblocks.size(); i++)
    {
        if (bytecode_basicblocks[i]->vm_insts.empty()) continue;
        bool is_func = bytecode_basicblocks[i]->vm_insts.front()->opcode == CVM::Opcode::FUNC;
        if (is_func)
            funcs_table[bytecode_basicblocks[i]->name] = cnt;
        else
            block_table[bytecode_basicblocks[i]->name] = cnt;
        //
        cnt += bytecode_basicblocks[i]->vm_insts.size();
    }
}

void COMPILER::BytecodeGenerator::fixJmp()
{
    for (int i = 1; i < bytecode_basicblocks.size(); i++)
    {
        for (auto inst : bytecode_basicblocks[i]->vm_insts)
        {
            if (!inOr(inst->opcode, CVM::Opcode::JMP, CVM::Opcode::JIF, CVM::Opcode::CALL)) continue;
            if (inst->opcode == CVM::Opcode::JMP)
            {
                auto *tmp   = static_cast<CVM::Jmp *>(inst);
                tmp->target = block_table[tmp->basic_block_name];
            }
            else if (inst->opcode == CVM::Opcode::JIF)
            {
                auto *tmp    = static_cast<CVM::Jif *>(inst);
                tmp->target1 = block_table[tmp->basic_block_name1];
                tmp->target2 = block_table[tmp->basic_block_name2];
            }
        }
    }
}

void COMPILER::BytecodeGenerator::fixCall()
{
    for (int i = 1; i < bytecode_basicblocks.size(); i++)
    {
        for (auto inst : bytecode_basicblocks[i]->vm_insts)
        {
            if (inst->opcode != CVM::Opcode::CALL) continue;
            auto *tmp   = static_cast<CVM::Call *>(inst);
            tmp->target = funcs_table[tmp->name];
        }
    }
}

void COMPILER::BytecodeGenerator::relocation()
{
    // we need to adjust the target where jmp or call will jump
    // and transform to vector at the end
    collectBlockMap();
    fixJmp();
    fixCall();
    for (auto block : bytecode_basicblocks)
    {
        if (block->name == ENTRY_FUNC) entry = vm_insts.size();
        for (auto inst : block->vm_insts)
        {
            vm_insts.push_back(inst);
        }
        if (block->name == entry_end_block_name) entry_end = vm_insts.size() - 1;
    }
}

std::string COMPILER::BytecodeGenerator::vmInstStr()
{
    std::string str;
    for (auto *block : bytecode_basicblocks)
    {
        for (auto inst : block->vm_insts)
        {
            if (inst == nullptr)
                str += "(optimized)\n";
            else
                str += inst->toString() + "\n";
        }
    }
    return str;
}

void COMPILER::BytecodeGenerator::parseVarArr(COMPILER::IRVar *var, std::vector<CVM::ArrIdx> &arr_idx)
{
    for (auto *x : var->index)
    {
        // type only IRConstant and IRVar
        auto *idx_const = as<IRConstant, IR::Tag::CONST>(x);
        auto *idx_var   = as<IRVar, IR::Tag::VAR>(x);
        if (idx_const != nullptr)
        {
            arr_idx.emplace_back(idx_const->value.as<long long>());
        }
        else if (idx_var != nullptr)
        {
            arr_idx.emplace_back(idx_var->ssaName());
        }
        else
            UNREACHABLE();
    }
}

void COMPILER::BytecodeGenerator::genGlobalVarDecl()
{
    for (auto inst : global_vars->insts)
    {
        auto *tmp = as<IRAssign, IR::Tag::ASSIGN>(inst);
        if (tmp == nullptr) 
          continue;
        genAssign(tmp);
    }
}
