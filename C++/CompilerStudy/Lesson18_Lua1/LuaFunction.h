
#ifndef LUA_FUNCTION_H
#define LUA_FUNCTION_H

#include "Function.h"
#include "LuaValue.h"
#include "Ast.h"

//======== LuaFunctionMeta ============
struct LuaFunctionMeta {
    int argCount;
    vector<LuaValue> constTable;
    vector<string> nameTable;
    StmtNodePtr body;

    LuaFunctionMeta(): argCount(0){}
    int getConstIndex(const LuaValue& v);
    int getNameIndex(const string& name);
};
typedef shared_ptr<LuaFunctionMeta> LuaFunctionMetaPtr;
//======== LuaFunction ============
class LuaFunction:
    public IFunction {
public:
    static LuaFunction* create(LuaFunctionMetaPtr meta, const vector<LuaValue>& upValues) {
        return new LuaFunction(meta, upValues);
    }

    LuaValue& getLocal(int idx);
    LuaValue& getUpValue(int idx);
    const vector<LuaValue>& getArgs() const { return m_args; }

    const LuaFunctionMeta* getMeta() const { return m_meta.get(); }

    virtual void call(const vector<LuaValue>& args, vector<LuaValue>& rets);

private:
    LuaFunction(LuaFunction& o);
    LuaFunction& operator = (const LuaFunction& o);
    LuaFunction(LuaFunctionMetaPtr meta, const vector<LuaValue>& upValues);
    ~LuaFunction();

private:
    LuaFunctionMetaPtr m_meta;
    vector<LuaValue> m_locals;
    vector<LuaValue> m_upValues;
    vector<LuaValue> m_args;
};
typedef shared_ptr<LuaFunction> LuaFunctionPtr;

#endif
