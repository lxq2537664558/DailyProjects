
#include "pch.h"

#include "LuaLibs.h"

int index_LuaValue2Int(const LuaValue& v, int len) {
    int r = (int)v.getNumber();
    if (r > 0) --r;
    else r += len;
    return r;
}

static void string_byte(const vector<LuaValue>& args, vector<LuaValue>& rets) {
    const char* s = args[0].getString()->buf();
    int len = args[0].getString()->size();
    int i = 0;
    if (args.size() >= 2) i = index_LuaValue2Int(args[1], len);
    int j = i;
    if (args.size() >= 3) j = index_LuaValue2Int(args[2], len);

    if (i > j) return;
    i = max(i, 0);
    j = min(j, len - 1);
    for (; i <= j; ++i) {
        rets.push_back(LuaValue(NumberType(s[i])));
    }
}
static void string_char(const vector<LuaValue>& args, vector<LuaValue>& rets) {
    string s;
    for (int i = 0; i < (int)args.size(); ++i) {
        s.push_back((char)args[i].getNumber());
    }
    rets.push_back(LuaValue(s.c_str()));
}
static void string_format(const vector<LuaValue>& args, vector<LuaValue>& rets) {
    const char *fmt = args[0].getString()->buf();
    string r, buf;
    int i = 1;
    for (; *fmt; ++fmt) {
        if (*fmt == '%') {
            r += buf;
            buf.clear();

            buf.push_back(*fmt);
            while (strchr("dsf", *++fmt) == NULL) buf.push_back(*fmt);
            buf.push_back(*fmt);
            LuaValue v;
            if (i < (int)args.size()) v = args[i];

            switch (*fmt) {
                case 'd': r += format(buf.c_str(), (int)v.getNumber()); break;
                case 's': r += format(buf.c_str(), v.getString()); break;
                case 'f': r += format(buf.c_str(), v.getNumber()); break;
                default: ASSERT(0); break;
            }

            ++i;
            buf.clear();
        }
        else {
            buf.push_back(*fmt);
        }
    }
    r += buf;
    rets.push_back(LuaValue(r.c_str()));
}
static void string_len(const vector<LuaValue>& args, vector<LuaValue>& rets) {
    rets.push_back(LuaValue(NumberType(args[0].getSize())));
}
static void string_lower(const vector<LuaValue>& args, vector<LuaValue>& rets) {
    auto str = args[0].getString();
    vector<char> r;
    for (int i = 0; i < str->size(); ++i) r.push_back(::tolower(str->buf()[i]));
    rets.push_back(r.empty() ? LuaValue("") : LuaValue(&r[0], (int)r.size()));
}
static void string_upper(const vector<LuaValue>& args, vector<LuaValue>& rets) {
    auto str = args[0].getString();
    vector<char> r;
    for (int i = 0; i < str->size(); ++i) r.push_back(::toupper(str->buf()[i]));
    rets.push_back(r.empty() ? LuaValue("") : LuaValue(&r[0], (int)r.size()));
}
static void string_rep(const vector<LuaValue>& args, vector<LuaValue>& rets) {
    auto str = args[0].getString();
    int n = (int)args[1].getNumber();
    vector<char> r;
    for (int i = 0; i < n; ++i) r.insert(r.end(), str->buf(), str->buf() + str->size());
    rets.push_back(r.empty() ? LuaValue("") : LuaValue(&r[0], (int)r.size()));
}
static void string_reverse(const vector<LuaValue>& args, vector<LuaValue>& rets) {
    auto str = args[0].getString();
    vector<char> r(str->buf(), str->buf() + str->size());
    std::reverse(r.begin(), r.end());
    rets.push_back(r.empty() ? LuaValue("") : LuaValue(&r[0], (int)r.size()));
}
static void string_sub(const vector<LuaValue>& args, vector<LuaValue>& rets) {
    auto str = args[0].getString();
    int i = index_LuaValue2Int(args[1], str->size());
    int j = str->size() - 1;
    if (args.size() >= 3) j = index_LuaValue2Int(args[2], str->size());
    if (i < 0 || i >= str->size() || i > j) rets.push_back(LuaValue(""));
    else rets.push_back(LuaValue(str->buf() + i, j - i + 1));
}

extern void openLib_string() {
    auto table = LuaTable::create();
    LuaVM::instance()->getGlobalTable()->set(LuaValue("string"), LuaValue(table));

#define ENTRY(name) {#name, &string_##name}
    CFuncEntry entries[] = {
        ENTRY(byte), ENTRY(char), ENTRY(format),
        ENTRY(len), ENTRY(lower), ENTRY(upper),
        ENTRY(rep), ENTRY(reverse), ENTRY(sub),
    };
#undef ENTRY
    for (auto &entry : entries) table->set(LuaValue(entry.name), LuaValue(CFunction::create(entry.func)));
}
