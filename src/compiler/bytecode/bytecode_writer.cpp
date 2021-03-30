#include "bytecode_writer.h"

void COMPILER::BytecodeWriter::writeInsts()
{
    writeHeader();
    for (auto *inst : vm_insts)
    {
        writeOpcode(inst->opcode);
        cur_inst = inst;
        switch (inst->opcode)
        {
            case CVM::Opcode::ADD:
            case CVM::Opcode::SUB:
            case CVM::Opcode::MUL:
            case CVM::Opcode::DIV:
            case CVM::Opcode::MOD:
            case CVM::Opcode::EXP:
            case CVM::Opcode::BAND:
            case CVM::Opcode::BOR:
            case CVM::Opcode::BXOR:
            case CVM::Opcode::SHL:
            case CVM::Opcode::SHR:
            case CVM::Opcode::LOR:
            case CVM::Opcode::NE:
            case CVM::Opcode::EQ:
            case CVM::Opcode::LT:
            case CVM::Opcode::LE:
            case CVM::Opcode::GT:
            case CVM::Opcode::GE:
            case CVM::Opcode::LAND: writeBinary(); break;
            case CVM::Opcode::LNOT:
            case CVM::Opcode::BNOT: writeUnary(); break;
            case CVM::Opcode::LOADA: writeLoadA(); break;
            case CVM::Opcode::LOADXA: writeLoadXA(); break;
            case CVM::Opcode::LOADI: writeLoad<int>(); break;
            case CVM::Opcode::STOREI: writeStore<int>(); break;
            case CVM::Opcode::LOADD: writeLoad<double>(); break;
            case CVM::Opcode::STORED: writeStore<double>(); break;
            case CVM::Opcode::LOADS: writeLoad<std::string>(); break;
            case CVM::Opcode::STORES: writeStore<std::string>(); break;
            case CVM::Opcode::STOREA: writeStoreA(); break;
            case CVM::Opcode::LOADX: writeLoadX(); break;
            case CVM::Opcode::STOREX: writeStoreX(); break;
            case CVM::Opcode::CALL: writeCall(); break;
            case CVM::Opcode::FUNC: writeFunc(); break;
            case CVM::Opcode::ARG: writeArg(); break;
            case CVM::Opcode::PARAM: writeParam(); break;
            case CVM::Opcode::RET: writeRet(); break;
            case CVM::Opcode::JMP: writeJmp(); break;
            case CVM::Opcode::JIF: writeJif(); break;
            default: ERROR("unsupported inst"); break;
        }
    }
}

void COMPILER::BytecodeWriter::writeHeader()
{
    // magic number
    writeByte(0xc2);
    // version
    writeByte(0x01);
    // entry point
    writeInt(entry);
    // main end
    writeInt(entry_end);
    writeInt(global_var_len);
}

void COMPILER::BytecodeWriter::writeByte(unsigned char val)
{
    buffer.push_back(val);
}

void COMPILER::BytecodeWriter::writeInt(long long val)
{
    auto x = reinterpret_cast<unsigned long long &>(val);
    for (int i = 0; i < 8; i++)
    {
        writeByte(x & 0xff);
        x >>= 8;
    }
}

void COMPILER::BytecodeWriter::writeDouble(double val)
{
    writeInt(reinterpret_cast<long long &>(val));
}

void COMPILER::BytecodeWriter::writeString(const std::string &val)
{
    writeInt(val.size());
    for (auto x : val)
    {
        writeByte(x);
    }
}

void COMPILER::BytecodeWriter::writeToFile()
{
    std::ofstream out;
    out.open(filename, std::ios::out | std::ios::binary);
    for (auto x : buffer)
    {
        out << x;
    }
    out.flush();
    out.close();
}

void COMPILER::BytecodeWriter::writeOpcode(CVM::Opcode opcode)
{
    writeByte(CVM::opcode2UChar(opcode));
}

void COMPILER::BytecodeWriter::writeBinary()
{
    auto *tmp = static_cast<CVM::Binary *>(cur_inst);
    writeByte(tmp->reg_idx1);
    writeByte(tmp->reg_idx2);
}

void COMPILER::BytecodeWriter::writeUnary()
{
    auto *tmp = static_cast<CVM::Unary *>(cur_inst);
    writeByte(tmp->reg_idx);
    if (tmp->type == CVM::ArgType::MAP)
    {
        writeStringTag();
        writeString(tmp->name);
    }
    else
    {
        // bnot int only
        // todo: lnot.
        writeIntTag();
        writeInt(tmp->value.as<int>());
    }
}

void COMPILER::BytecodeWriter::writeLoadA()
{
    auto *tmp = static_cast<CVM::LoadA *>(cur_inst);
    writeByte(tmp->reg_idx);
    writeInt(tmp->array.size());
    for (auto value : tmp->array)
    {
        if (!value.hasValue())
        {
            writeEmptyTag();
            continue;
        }
        if (value.is<int>())
        {
            writeIntTag();
            writeInt(value.as<int>());
        }
        else if (value.is<double>())
        {
            writeDoubleTag();
            writeDouble(value.as<double>());
        }
        else if (value.is<std::string>())
        {
            writeStringTag();
            writeString(value.as<std::string>());
        }
    }
}

void COMPILER::BytecodeWriter::writeLoadXA()
{
    auto *tmp = static_cast<CVM::LoadXA *>(cur_inst);
    writeByte(tmp->reg_idx);
    writeInt(tmp->index);
    writeString(tmp->name);
}

void COMPILER::BytecodeWriter::writeLoadX()
{
    // LOAD DEST SRC
    auto *tmp = static_cast<CVM::LoadX *>(cur_inst);
    writeByte(tmp->reg_idx);
    writeString(tmp->name);
    writeInt(tmp->index.size());
    for (auto idx : tmp->index)
    {
        if (std::holds_alternative<int>(idx))
        {
            writeIntTag();
            writeInt(std::get<int>(idx));
        }
        else
        {
            writeStringTag();
            writeString(std::get<std::string>(idx));
        }
    }
}

template<typename T>
void COMPILER::BytecodeWriter::writeLoad()
{
    if constexpr (std::is_same<T, int>())
    {
        auto *tmp = static_cast<CVM::LoadI *>(cur_inst);
        writeByte(tmp->reg_idx);
        writeInt(tmp->val);
    }
    else if constexpr (std::is_same<T, double>())
    {
        auto *tmp = static_cast<CVM::LoadD *>(cur_inst);
        writeByte(tmp->reg_idx);
        writeDouble(tmp->val);
    }
    else if constexpr (std::is_same<T, std::string>())
    {
        auto *tmp = static_cast<CVM::LoadS *>(cur_inst);
        writeByte(tmp->reg_idx);
        writeString(tmp->val);
    }
    else
        UNREACHABLE();
}

void COMPILER::BytecodeWriter::writeStoreX()
{
    // STORE DEST SRC
    auto *tmp = static_cast<CVM::StoreX *>(cur_inst);
    writeString(tmp->name);
    writeInt(tmp->index.size());
    for (auto idx : tmp->index)
    {
        if (std::holds_alternative<int>(idx))
        {
            writeIntTag();
            writeInt(std::get<int>(idx));
        }
        else
        {
            writeStringTag();
            writeString(std::get<std::string>(idx));
        }
    }
    writeByte(tmp->reg_idx);
}

void COMPILER::BytecodeWriter::writeStoreA()
{
    // STOREA a[1][b] = 3.14
    auto *tmp = static_cast<CVM::StoreA *>(cur_inst);
    writeString(tmp->name);
    writeInt(tmp->index.size());
    // [1][b]
    for (auto idx : tmp->index)
    {
        if (std::holds_alternative<int>(idx))
        {
            writeIntTag();
            writeInt(std::get<int>(idx));
        }
        else
        {
            writeStringTag();
            writeString(std::get<std::string>(idx));
        }
    }
    // 3.14
    if (tmp->value.is<int>())
    {
        writeIntTag();
        writeInt(tmp->value.as<int>());
    }
    else if (tmp->value.is<double>())
    {
        writeDoubleTag();
        writeDouble(tmp->value.as<double>());
    }
    else if (tmp->value.is<std::string>())
    {
        writeStringTag();
        writeString(tmp->value.as<std::string>());
    }
}

template<typename T>
void COMPILER::BytecodeWriter::writeStore()
{
    if constexpr (std::is_same<T, int>())
    {
        auto *tmp = static_cast<CVM::StoreI *>(cur_inst);
        writeString(tmp->name);
        writeInt(tmp->val);
    }
    else if constexpr (std::is_same<T, double>())
    {
        auto *tmp = static_cast<CVM::StoreD *>(cur_inst);
        writeString(tmp->name);
        writeDouble(tmp->val);
    }
    else if constexpr (std::is_same<T, std::string>())
    {
        auto *tmp = static_cast<CVM::StoreS *>(cur_inst);
        writeString(tmp->name);
        writeString(tmp->val);
    }
    else
        UNREACHABLE();
}

void COMPILER::BytecodeWriter::writeArg()
{
    auto *arg = static_cast<CVM::Arg *>(cur_inst);
    // write target type
    if (arg->type == CVM::ArgType::MAP)
    {
        writeByte(0);
        writeString(arg->name);
    }
    else if (arg->type == CVM::ArgType::RAW)
    {
        writeByte(1);
        // int = 0 / double = 1 / string = 2
        if (arg->value.is<int>())
        {
            writeIntTag();
            writeInt(arg->value.as<int>());
        }
        else if (arg->value.is<double>())
        {
            writeDoubleTag();
            writeDouble(arg->value.as<double>());
        }
        else if (arg->value.is<std::string>())
        {
            writeStringTag();
            writeString(arg->value.as<std::string>());
        }
    }
}

void COMPILER::BytecodeWriter::writeCall()
{
    auto *tmp = static_cast<CVM::Call *>(cur_inst);
    writeInt(tmp->target); // func line no
}

void COMPILER::BytecodeWriter::writeFunc()
{
    auto *tmp = static_cast<CVM::Func *>(cur_inst);
    writeByte(tmp->param_count); // argument count
}

void COMPILER::BytecodeWriter::writeParam()
{
    auto *tmp = static_cast<CVM::Param *>(cur_inst);
    writeString(tmp->name);
}

void COMPILER::BytecodeWriter::writeRet()
{
    // there is nothing to do...
}

void COMPILER::BytecodeWriter::writeJmp()
{
    auto *tmp = static_cast<CVM::Jmp *>(cur_inst);
    writeInt(tmp->target);
}

void COMPILER::BytecodeWriter::writeJif()
{
    auto *tmp = static_cast<CVM::Jif *>(cur_inst);
    writeInt(tmp->target1);
    writeInt(tmp->target2);
}

void COMPILER::BytecodeWriter::writeIntTag()
{
    writeByte(0);
}

void COMPILER::BytecodeWriter::writeDoubleTag()
{
    writeByte(1);
}

void COMPILER::BytecodeWriter::writeStringTag()
{
    writeByte(2);
}

void COMPILER::BytecodeWriter::writeEmptyTag()
{
    writeByte(3);
}
