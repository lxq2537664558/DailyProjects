#ifndef UTILS_H
#define UTILS_H

#include <exception>
#include <utility>
#include <functional>
#include <sstream>
//////////////////////////////
extern string format(const char *fmt, ...);

#define _TO_STRING(a) #a
#define TO_STRING(a) _TO_STRING(a)
#define _CONN(a, b) a##b
#define CONN(a, b) _CONN(a, b)

#define DISABLE_COPY(type) type(const type&) = delete; type& operator = (const type&) = delete
#define ENABLE_COPY(type) 
//////////////////////////////
class Exception: public exception {
    ENABLE_COPY(Exception)
public:
    Exception(const char *describ, const char *file, int line);
    const char* what() const throw() { return mWhat.c_str(); }
    template<typename T>
    Exception& operator << (pair<const char*, T> varInfo) {
        ostringstream os;
        os << "\t\t" << varInfo.first << ':' << varInfo.second << '\n';
        mWhat += os.str();
        return *this;
    }
    Exception& operator << (const char *s) { return *this; }
protected:
    Exception();
protected:
    string mWhat;
};

class PosixException: public Exception {
    ENABLE_COPY(PosixException)
public:
    PosixException(int err, const char *describ, const char *file, int line);
};

extern const char *_ENSURE_CTX_A;
extern const char *_ENSURE_CTX_B;
#define _ENSURE_CTX_A(v) make_pair(#v, v) << _ENSURE_CTX_B
#define _ENSURE_CTX_B(v) make_pair(#v, v) << _ENSURE_CTX_A
#define ENSURE(b) if (b); else throw Exception(#b, __FILE__, __LINE__) << _ENSURE_CTX_A
#define P_ENSURE_ERR(b, err) if (b); else throw PosixException(err, #b, __FILE__, __LINE__) << _ENSURE_CTX_A
#define P_ENSURE(b) P_ENSURE_ERR(b, errno)
#define P_ENSURE_R(exp) for (int err = exp; err != 0; err = 0) throw PosixException(err, #exp, __FILE__, __LINE__) << _ENSURE_CTX_A

//////////////////////////////
class ScopeGuard {
    DISABLE_COPY(ScopeGuard);
public:
    ScopeGuard(function<void()> f): mF(f) {}
    ~ScopeGuard() { if (mF) mF(); }
    void dismiss() { mF = nullptr; }
private:
    function<void()> mF;
};

#define ON_EXIT_SCOPE(f) ScopeGuard CONN(__scopeVar_, __LINE__)(f)
//////////////////////////////

#endif