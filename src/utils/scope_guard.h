#pragma once

#pragma warning ( push )
// warning C4180: qualifier applied to function type has no meaning; ignored
#pragma warning( disable : 4180 )

template <class T>
class RefHolder
{
    T& ref_;
public:
    RefHolder(T& ref) : ref_(ref) {}
    operator T& () const
        { return ref_; }
};

template <class T>
inline RefHolder<T> ByRef(T& t)
{
    return RefHolder<T>(t);
}

class ScopeGuardImplBase
{
public:

    void Dismiss() const throw()
        { dismissed_ = true; }

protected:

    ScopeGuardImplBase()
        : dismissed_(false)
    {}

    ScopeGuardImplBase(const ScopeGuardImplBase& rhs)
        : dismissed_(rhs.dismissed_)
    {
        rhs.Dismiss();
    }

    ~ScopeGuardImplBase() {}; // non-virtual dtor used since we never use objects by pointer on this class object.

    mutable bool dismissed_;

private:

    ScopeGuardImplBase& operator=(const ScopeGuardImplBase&);
};

typedef const ScopeGuardImplBase& ScopeGuard;

/*
    Note: fun() must not throw exceptions.
    They will be caught silently anyway since dtors must not throw exceptions.
*/
template <typename Fun, typename Parm>
class ScopeGuardImpl1 : public ScopeGuardImplBase
{
public:

    ScopeGuardImpl1(const Fun& fun, const Parm& parm)
        : fun_(fun), parm_(parm) 
    {}

    ~ScopeGuardImpl1()
    {
        if (!dismissed_) {
            try {
                fun_(parm_);
            } catch(...) {
                // hide all exceptions.
            }
        }
    }

private:

    Fun fun_;
    const Parm parm_;
};

template <typename Fun, typename Parm>
ScopeGuardImpl1<Fun, Parm>
MakeGuard(const Fun& fun, const Parm& parm)
{
    return ScopeGuardImpl1<Fun, Parm>(fun, parm);
}

/*
    Note: memFun must not throw exceptions.
    They will be caught silently anyway since dtors must not throw exceptions.
*/
template <class Obj, typename MemFun>
class ObjScopeGuardImpl0 : public ScopeGuardImplBase
{
public:
    ObjScopeGuardImpl0(Obj& obj, MemFun memFun)
        : obj_(obj), memFun_(memFun) 
    {}

    ~ObjScopeGuardImpl0()
    {
        if (!dismissed_) {
            try {
                (obj_.*fun_)();
            } catch(...) {
                // hide all exceptions.
            }
        }
    }

private:

    Obj& obj_;
    MemFun memFun_;
};

template <typename Obj, typename MemFun>
ObjScopeGuardImpl0<Obj, MemFun>
MakeObjGuard(Obj& obj, MemFun fun)
{
    return ObjScopeGuardImpl0<Obj, MemFun>(obj, fun);
}

#define _CONCAT_MACRO(x,y) x##y
#define CONCAT_MACRO(x,y) _CONCAT_MACRO(x,y)
#define ANONYMOUS_GUARD_NAME CONCAT_MACRO(scope_guard, __LINE__)
#define ON_BLOCK_EXIT(...) ScopeGuard ANONYMOUS_GUARD_NAME = MakeGuard(__VA_ARGS__); ANONYMOUS_GUARD_NAME // prevent warning about unused variable.

#pragma warning (pop)
