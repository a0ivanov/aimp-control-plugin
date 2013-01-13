// Copyright (c) 2013, Alexey Ivanov

#include "stdafx.h"
#include "rpc/value.h"
#include "rpc/exception.h"
#include <cassert>
#include <sstream>

namespace Rpc
{

// debug function, used to determine unhandled type(for example when new type is added but not handled in all places)
void handleUnknownType()
{
    assert(!"unknown Value type");
    throw Exception("type error: unknown type", TYPE_ERROR);
}

const char* asString(Value::TYPE type)
{
    const char* type_name = nullptr;
    switch (type) {
    case Value::TYPE_NONE:
        type_name = "none";
        break;
    case Value::TYPE_NULL:
        type_name = "null";
        break;
    case Value::TYPE_BOOL:
        type_name = "boolean";
        break;
    case Value::TYPE_INT:
        type_name = "int";
        break;
    case Value::TYPE_UINT:
        type_name = "uint";
        break;
    case Value::TYPE_DOUBLE:
        type_name = "double";
        break;
    case Value::TYPE_STRING:
        type_name = "string";
        break;
    case Value::TYPE_ARRAY:
        type_name = "array";
        break;
    case Value::TYPE_OBJECT:
        type_name = "object";
        break;
    default:
        type_name = "unknown";
        handleUnknownType();
        break;
    }
    return type_name;
}

void throwTypeErrorException(Value::TYPE expected, Value::TYPE current)
{
    std::ostringstream os;
    os << "type error: expected " << asString(expected) << ", have " << asString(current);
    throw Exception(os.str(), TYPE_ERROR);
}

void throwObjectAccessException(const char* member_name)
{
    std::ostringstream os;
    os << "object access error: member \"" << member_name << "\" does not exist";
    throw Exception(os.str(), OBJECT_ACCESS_ERROR);
}

Value::Value()
    :
    type_(TYPE_NONE)
{}

Value::Value(const Value::Null&)
    :
    type_(TYPE_NULL)
{}

Value::Value(bool value)
    :
    type_(TYPE_BOOL)
{
    value_.bool_ = value;
}

Value::Value(int value)
    :
    type_(TYPE_INT)
{
    value_.int_ = value;
}

Value::Value(unsigned int value)
    :
    type_(TYPE_UINT)
{
    value_.uint_ = value;
}

Value::Value(double value)
    :
    type_(TYPE_DOUBLE)
{
    value_.double_ = value;
}

Value::Value(const char* value)
    :
    type_(TYPE_STRING)
{
    value_.string_ = new String(value);
}

Value::Value(const String& value)
    :
    type_(TYPE_STRING)
{
    value_.string_ = new String(value);
}

Value::Value(const Value::Array& value)
    :
    type_(TYPE_ARRAY)
{
    value_.array_ = new Array(value);
}

Value::Value(const Value::Object& value)
    :
    type_(TYPE_OBJECT)
{
    value_.object_ = new Object(value);
}

Value::String* copyString(const Value::String* rhs)
{
    assert(rhs);
    return new Value::String(*rhs);
}

Value::Array* copyArray(const Value::Array* rhs)
{
    assert(rhs);
    return new Value::Array(*rhs);
}

Value::Object* copyObject(const Value::Object* rhs)
{
    assert(rhs);
    return new Value::Object(*rhs);
}

Value::Value(const Value& rhs)
    :
    type_(rhs.type_)
{
    switch (type_) {
    case TYPE_NONE:
    case TYPE_NULL:
        break;
    case TYPE_BOOL:
        value_.bool_ = rhs.value_.bool_;
        break;
    case TYPE_INT:
        value_.int_ = rhs.value_.int_;
        break;
    case TYPE_UINT:
        value_.uint_ = rhs.value_.uint_;
        break;
    case TYPE_DOUBLE:
        value_.double_ = rhs.value_.double_;
        break;
    case TYPE_STRING:
        value_.string_ = copyString(rhs.value_.string_);
        break;
    case TYPE_ARRAY:
        value_.array_ = copyArray(rhs.value_.array_);
        break;
    case TYPE_OBJECT:
        value_.object_ = copyObject(rhs.value_.object_);
        break;
    default:
        handleUnknownType();
        break;
    }
}

void Value::reset()
{
    switch (type_) {
    case TYPE_NONE:
    case TYPE_NULL:
    case TYPE_BOOL:
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_DOUBLE:
        break;
    case TYPE_STRING:
        delete value_.string_;
        break;
    case TYPE_ARRAY:
        delete value_.array_;
        break;
    case TYPE_OBJECT:
        delete value_.object_;
        break;
    default:
        handleUnknownType();
        break;
    }

    type_ = TYPE_NONE;
}

void Value::swap(Value& rhs)
{
    using std::swap;
    swap(type_, rhs.type_);
    swap(value_, rhs.value_);
}

Value& Value::operator=(const Value& rhs)
{
    Value(rhs).swap(*this);
    return *this;
}

Value& Value::operator=(const Value::Null& value)
{
    Value(value).swap(*this);
    return *this;
}

Value& Value::operator=(bool value)
{
    Value(value).swap(*this);
    return *this;
}

Value& Value::operator=(int value)
{
    Value(value).swap(*this);
    return *this;
}

Value& Value::operator=(unsigned int value)
{
    Value(value).swap(*this);
    return *this;
}

Value& Value::operator=(double value)
{
    Value(value).swap(*this);
    return *this;
}

Value& Value::operator=(const char* value)
{
    Value(value).swap(*this);
    return *this;
}

Value& Value::operator=(const Value::String& value)
{
    Value(value).swap(*this);
    return *this;
}

Value& Value::operator=(const Value::Array& value)
{
    Value(value).swap(*this);
    return *this;
}

Value& Value::operator=(const Value::Object& value)
{
    Value(value).swap(*this);
    return *this;
}

void Value::ensureTypeIsNoneOrEquals(TYPE type)
{
    if (type == type_) {
        // do nothing
    } else if (type_ == TYPE_NONE) {
        switch (type) {
        case TYPE_NONE:
            Value().swap(*this);
            break;
        case TYPE_NULL:
            Value( Null() ).swap(*this);
            break;
        case TYPE_BOOL:
            Value(false).swap(*this);
            break;
        case TYPE_INT:
        case TYPE_UINT:
            Value(0).swap(*this);
            break;
        case TYPE_DOUBLE:
            Value(0.0).swap(*this);
            break;
        case TYPE_STRING:
            Value( String() ).swap(*this);
            break;
        case TYPE_ARRAY:
            Value( Array() ).swap(*this);
            break;
        case TYPE_OBJECT:
            Value( Object() ).swap(*this);
            break;
        default:
            handleUnknownType();
            break;
        }
    } else {
        throwTypeErrorException(type, type_);
    }
}

void Value::assertTypeEquals(TYPE type) const
{
    if (type != type_) {
        throwTypeErrorException(type, type_);
    }
}

Value::operator bool&()
{
    ensureTypeIsNoneOrEquals(TYPE_BOOL);
    return value_.bool_;
}

Value::operator bool() const
{
    assertTypeEquals(TYPE_BOOL);
    return value_.bool_;
}

Value::operator int&()
{
    ensureTypeIsNoneOrEquals(TYPE_INT);
    return value_.int_;
}

Value::operator unsigned int&()
{
    ensureTypeIsNoneOrEquals(TYPE_UINT);
    return value_.uint_;
}

Value::operator int() const
{
    assertTypeEquals(TYPE_INT);
    return value_.int_;
}

Value::operator unsigned int() const
{
    assertTypeEquals(TYPE_UINT);
    return value_.uint_;
}

Value::operator double&()
{
    ensureTypeIsNoneOrEquals(TYPE_DOUBLE);
    return value_.double_;
}

Value::operator double() const
{
    assertTypeEquals(TYPE_DOUBLE);
    return value_.double_;
}

Value::operator String&()
{
    ensureTypeIsNoneOrEquals(TYPE_STRING);
    assert(value_.string_);
    return *value_.string_;
}

Value::operator String const&() const
{
    assertTypeEquals(TYPE_STRING);
    assert(value_.string_);
    return *value_.string_;
}

bool Value::operator==(const char* value) const
{
    if (type_ == TYPE_STRING) {
        assert(value_.string_);
        return *value_.string_ == value;
    }
    return false;
}

bool Value::operator==(const String& value) const
{
    if (type_ == TYPE_STRING) {
        assert(value_.string_);
        return *value_.string_ == value;
    }
    return false;
}

void Value::assertIndexIsInRange(int index) const
{
    assertTypeEquals(TYPE_ARRAY);
    assert(value_.array_);
    const Array& array = *value_.array_;
    if (index < 0 || (size_t)index >= array.size() ) {
        std::ostringstream os;
        os << "array index out of bound: array size " << array.size() << ", index " << index;
        throw Exception(os.str(), INDEX_RANGE_ERROR);
    }
}

Value& Value::operator[](int index)
{
    if (type_ == TYPE_NONE) {
        // create array with 1 element if type is none.
        ensureTypeIsNoneOrEquals(TYPE_ARRAY);
        setSize(1);
    }
    assertIndexIsInRange(index);
    return (*value_.array_)[index];
}

const Value& Value::operator[](int index) const
{
    assertIndexIsInRange(index);
    return (*value_.array_)[index];
}

const size_t Value::size() const
{
    switch (type_) {
    case TYPE_ARRAY:
        return value_.array_->size();
    case TYPE_OBJECT:
        return value_.object_->size();
    default:
        break;
    }

    std::ostringstream os;
    os << "type error: expected TYPE_ARRAY or TYPE_OBJECT, have " << asString(type_);
    throw Exception(os.str(), TYPE_ERROR);
}

void Value::setSize(size_t size)
{
    ensureTypeIsNoneOrEquals(TYPE_ARRAY);
    assert(value_.array_);
    Array& array = *value_.array_;
    array.resize(size);
}

const Value* Value::lookup(const Value::String& name) const
{
    assert(type_ == TYPE_OBJECT);

    const Object& object = *value_.object_;
    const auto it = object.find(name);
    if ( it != object.end() ) {
        return &it->second;
    }
    return nullptr;
}

Value& Value::operator[](const String& name)
{
    ensureTypeIsNoneOrEquals(TYPE_OBJECT);
    Object& object = *value_.object_;
    return object[name];
}

const Value& Value::operator[](const String& name) const
{
    assertTypeEquals(TYPE_OBJECT);
    const Value* value = lookup(name);
    if (!value) {
        throwObjectAccessException( name.c_str() );
    }
    return *value;
}

Value& Value::operator[](const char* name)
{
    // till I will find how to perform efficient search by char* in map<std::string> this function will create temp string object.
    return operator[]( String(name) );
}

const Value& Value::operator[](const char* name) const
{
    // till I will find how to perform efficient search by char* in map<std::string> this function will create temp string object.
    return operator[]( String(name) );
}

Value::Object::const_iterator Value::getObjectMembersBegin() const
{
    assertTypeEquals(TYPE_OBJECT);
    return value_.object_->begin();
}

Value::Object::const_iterator Value::getObjectMembersEnd() const
{
    assertTypeEquals(TYPE_OBJECT);
    return value_.object_->end();
}

bool Value::isMember(const String& name) const
{
    if (type_ == TYPE_OBJECT) {
        return (lookup(name) != nullptr);
    }
    return false;
}

bool Value::isMember(const char* name) const
{
    // till I will find how to perform efficient search by char* in map<std::string> this function will create temp string object.
    return isMember( String(name) );
}

std::ostream& Value::toStream(std::ostream& os) const
{
    switch (type_) {
    case TYPE_NONE:
        ///??? what we can output for none type?
        assert(!"try send non-value to ostream");
        break;
    case TYPE_NULL:
        os << "null";
        break;
    case TYPE_BOOL:
        os << std::boolalpha << value_.bool_;
        break;
    case TYPE_INT:
        os << value_.int_;
        break;
    case TYPE_UINT:
        os << value_.uint_;
        break;
    case TYPE_DOUBLE:
        os << value_.double_;
        break;
    case TYPE_STRING:
        os << *value_.string_;
        break;
    case TYPE_ARRAY:
        {
        os << '[';
        const Array& array = *value_.array_;
        for (auto begin = array.begin(), end = array.end(),
                  it = begin;
                  it != end;
                  ++it
             )
        {
            if (it != begin) {
                os << ',';
            }
            os << *it;
        }
        os << ']';
        }
        break;
    case TYPE_OBJECT:
        {
        os << '{';
        const Object& object = *value_.object_;
        for (auto begin = object.begin(), end = object.end(),
                  it = begin;
                  it != end;
                  ++it
             )
        {
            if (it != begin) {
                os << ',';
            }
            os << '"' << it->first << '"';
            os << ':';
            os << it->second;
        }
        os << '}';
        }
        break;
    default:
        handleUnknownType();
        break;
    }
    return os;
}

} // namespace Rpc
