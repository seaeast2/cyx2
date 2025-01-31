#ifndef CVM_CFG_H
#define CVM_CFG_H

#include "../../utility/utility.hpp"
#include "basicblock.hpp"
#include "ir_instruction.hpp"

#include <algorithm>
#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>

namespace COMPILER
{
    class CFG
    {

      public:
        void simplifyCFG();
        void buildDominateTree(COMPILER::IRFunction *func);
        void transformToSSA(); // entry point
        void removeUnusedPhis(IRFunction *func);
        std::string iDomDetailStr() const;
        std::string dominanceFrontierStr() const;
        std::string cfgStr() const;

      public:
        std::vector<IRFunction *> funcs;

      private:
        void init(IRFunction *func);
        void clear();
        void dfs(BasicBlock *cur_basic_block);
        // lengauer-tarjan algorithm 
        void tarjan();
        void calcDominanceFrontier(COMPILER::IRFunction *func);
        // disjoint set
        // 경로 압축을 적용한 block의 최소값 dfnum의 준지배자 찾기
        COMPILER::BasicBlock *find(COMPILER::BasicBlock *block);
        // SSA construction
        void collectVarAssign(COMPILER::IRFunction *func);
        void insertPhiNode();
        void removeTrivialPhi(COMPILER::IRFunction *func);
        //
        void constantPropagation(COMPILER::IRFunction *func);
        void constantFolding(COMPILER::IRFunction *func);
        std::optional<CYX::Value> tryFindConstant(COMPILER::IRVar *var);
        //
        void destroyPhiNode(COMPILER::IRAssign *assign);
        // phi 함수 제거, register allocation 은 사용하지 않고 간단한 방식.
        void phiElimination(COMPILER::IRFunction *func);
        void deadCodeElimination(COMPILER::IRFunction *func);
        // rename
        void tryRename(COMPILER::IRFunction *func);
        void rename(BasicBlock *block);
        void renameIrArgs(IR *inst);
        void renameFuncCall(IRCall *inst);
        void renameVar(IRVar *var);
        int newId(const std::string &name, IRVar *def);
        std::pair<int, IRVar *> getId(const std::string &name);

      private:
        BasicBlock *entry{ nullptr };
        // dfs related
        // 깊이 우선 탐색으로 dfnum 지정
        std::vector<BasicBlock *> dfn;
        // node로 dfnum 검색
        std::unordered_map<BasicBlock *, int> dfn_map;
        std::unordered_map<BasicBlock *, BasicBlock *> father;
        std::unordered_map<BasicBlock *, bool> visited;
        // dominate tree related
        // <node,semidominator> : 특정노드의 준지배자 쌍
        std::unordered_map<BasicBlock *, BasicBlock *> sdom; // a.k.a semi, not strict.
        // immediate dominator : 직접 지배자
        std::unordered_map<BasicBlock *, BasicBlock *> idom; // the closest point of dominated point
        // 준지배자 숲. 책에서 bucket 과 동일
        std::unordered_map<BasicBlock *, std::unordered_set<BasicBlock *>> tree;
        // DF
        std::unordered_map<BasicBlock *, std::unordered_set<BasicBlock *>> dominance_frontier;
        // disjoint set (union find) related
        // <node, father> : 현재 노드의 부모 노드쌍
        std::unordered_map<BasicBlock *, BasicBlock *> disjoint_set;
        // <node, > : 부모의 부모??
        std::unordered_map<BasicBlock *, BasicBlock *> disjoint_set_val;
        // SSA construction.....
        std::unordered_map<std::string, std::unordered_set<BasicBlock *>> var_block_map;
        std::unordered_map<std::string, int> counter;
        std::unordered_map<std::string, std::stack<std::pair<int, IRVar *>>> stack;
        // for build new def-use chain.
        std::unordered_map<std::string, IRVar *> ssa_def_map;
    };
} // namespace COMPILER

#endif // CVM_CFG_H
