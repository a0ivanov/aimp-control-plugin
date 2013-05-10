// Copyright (c) 2013, Alexey Ivanov

#pragma once

#include <iosfwd>
#include <string>
#include <map>
#include <vector>

namespace Rpc
{


/*  Represents "interception" of XmlRpc value and JsonRpc value.
    This means that we can not use Base64 and DateTime types from XmlRpc.
*/
class Value
{
public:

    enum TYPE {
        TYPE_NONE,
        TYPE_NULL,
        TYPE_BOOL,
        TYPE_INT,
        TYPE_UINT,
        TYPE_DOUBLE,
        TYPE_STRING,
        TYPE_ARRAY,
        TYPE_OBJECT
    };

    typedef std::string String;
    typedef std::vector<Value> Array;
    typedef std::map<std::string, Value> Object;
    struct Null {};

    Value();
    Value(const Value& rhs);
    explicit Value(const Null&);
    explicit Value(bool value);
    explicit Value(int value);
    explicit Value(unsigned int value);
    explicit Value(double value);
    explicit Value(const char* value);
    explicit Value(const String& value);
    explicit Value(const Array& value);
    explicit Value(const Object& value);

    Value& operator=(const Value& rhs);
    Value& operator=(const Null&);
    Value& operator=(bool value);
    Value& operator=(int value);
    Value& operator=(unsigned int value);
    Value& operator=(double value);
    Value& operator=(const char* value);
    Value& operator=(const String& value);
    Value& operator=(const Array& value);
    Value& operator=(const Object& value);

    operator bool&();               // throws Exception
    operator bool() const;          // throws Exception
    operator int&();                // throws Exception
    operator int() const;           // throws Exception
    operator unsigned int&();       // throws Exception
    operator unsigned int() const;  // throws Exception
    operator double&();             // throws Exception
    operator double() const;        // throws Exception
    operator String&();             // throws Exception
    operator String const&() const; // throws Exception

    bool operator==(const char* value) const;
    bool operator==(const String& value) const;

    Value& operator[](int index);             // throws Exception
    const Value& operator[](int index) const; // throws Exception

    const size_t size() const; // throws Exception
    void setSize(size_t size); // throws Exception

    Value& operator[](const String& name);             // throws Exception
    const Value& operator[](const String& name) const; // throws Exception
    Value& operator[](const char* name);               // throws Exception
    const Value& operator[](const char* name) const;   // throws Exception

    Object::const_iterator getObjectMembersBegin() const;
    Object::const_iterator getObjectMembersEnd() const;

    bool isMember(const String& name) const;
    bool isMember(const char* name) const;

    //! destroys current value, sets type to TYPE_NONE.
    void reset();

    void swap(Value& rhs);

    std::ostream& toStream(std::ostream& os) const;

    TYPE type() const
        { return type_; }

    bool valid() const
        { return type_ != TYPE_NONE; }

private:

    void assertTypeEquals(TYPE type) const;     // throws Exception
    void ensureTypeIsNoneOrEquals(TYPE type);   // throws Exception
    void assertIndexIsInRange(int index) const; // throws Exception
    // does not perform check that current type is Object, caller must check it otself.
    const Value* lookup(const String& name) const;

    TYPE type_;

    union Value_ {
        bool bool_;
        int int_;
        unsigned int uint_;
        double double_;
        String* string_;
        Array* array_;
        Object* object_;
    };

    Value_ value_;
};


inline std::ostream& operator<<(std::ostream& os, const Value& value)
{
    value.toStream(os);
    return os;
}

} // namespace Rpc
