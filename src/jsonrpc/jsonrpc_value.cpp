#include "stdafx.h"
#include "xmlrpc/value.h"
#include "xmlrpc/parse_util.h"
#include "rpc/exception.h"
#include "utils/base64.h"

#ifndef MAKEDEPEND
# include <iostream>
# include <ostream>
# include <stdlib.h>
# include <stdio.h>
#endif

namespace XmlRpc {

static const char VALUE_TAG[]     = "<value>";
static const char VALUE_ETAG[]    = "</value>";

static const char NIL_TAG[]       = "<nil>";
static const char NIL_TAG_FULL[]  = "<nil/>";
static const char BOOLEAN_TAG[]   = "<boolean>";
static const char BOOLEAN_ETAG[]  = "</boolean>";
static const char DOUBLE_TAG[]    = "<double>";
static const char DOUBLE_ETAG[]   = "</double>";
static const char INT_TAG[]       = "<int>";
static const char I4_TAG[]        = "<i4>";
static const char I4_ETAG[]       = "</i4>";
static const char STRING_TAG[]    = "<string>";
static const char DATETIME_TAG[]  = "<dateTime.iso8601>";
static const char DATETIME_ETAG[] = "</dateTime.iso8601>";
static const char BASE64_TAG[]    = "<base64>";
static const char BASE64_ETAG[]   = "</base64>";

static const char ARRAY_TAG[]     = "<array>";
static const char DATA_TAG[]      = "<data>";
static const char DATA_ETAG[]     = "</data>";
static const char ARRAY_ETAG[]    = "</array>";

static const char STRUCT_TAG[]    = "<struct>";
static const char MEMBER_TAG[]    = "<member>";
static const char NAME_TAG[]      = "<name>";
static const char NAME_ETAG[]     = "</name>";
static const char MEMBER_ETAG[]   = "</member>";
static const char STRUCT_ETAG[]   = "</struct>";

// Format strings
const std::string kDoubleFormat("%f");

void Value::swap(Value& rhs)
{
    using std::swap;
    swap(_type, rhs._type);
    swap(_value, rhs._value);
}

// Clean up
void Value::invalidate()
{
    switch (_type)
    {
    case TypeString:    delete _value.asString; break;
    case TypeDateTime:  delete _value.asTime;   break;
    case TypeBase64:    delete _value.asBinary; break;
    case TypeArray:     delete _value.asArray;  break;
    case TypeStruct:    delete _value.asStruct; break;
    default: break;
    }
    _type = TypeInvalid;
    _value.asBinary = 0;
}

// Type checking
void Value::assertTypeOrInvalid(Type t)
{
    if (_type == TypeInvalid) {
        _type = t;
        switch (_type)
        {    // Ensure there is a valid value for the type
        case TypeString:   _value.asString = new std::string(); break;
        case TypeDateTime: _value.asTime = new struct tm();     break;
        case TypeBase64:   _value.asBinary = new BinaryData();  break;
        case TypeArray:    _value.asArray = new ValueArray();   break;
        case TypeStruct:   _value.asStruct = new ValueStruct(); break;
        default:           _value.asBinary = 0; break;
        }
    } else if (_type != t) {
        throw Rpc::Exception("type error", Rpc::TYPE_ERROR);
    }
}

// Type checking
void Value::assertType(Type t) const
{
    if (_type != t) {
        throw Rpc::Exception("type error", Rpc::TYPE_ERROR);
    }
}

Value const& Value::operator[](int i) const {
    assertArray(i + 1);
    return _value.asArray->at(i);
}

Value const& Value::operator[](std::string const& k) const { // throws Rpc::Exception
    if ( !hasMember(k) ) {
        throw Rpc::Exception("type error: const struct has no requested member", Rpc::TYPE_ERROR);
    }

    return (*_value.asStruct)[k];;
}

Value const& Value::operator[](const char* k) const { // throws Rpc::Exception
    if ( !hasMember(k) ) {
        throw Rpc::Exception("type error: const struct has no requested member", Rpc::TYPE_ERROR);
    }

    return (*_value.asStruct)[k];
}

void Value::assertArray(int size) const
{
    if (_type != TypeArray) {
        throw Rpc::Exception("type error: expected an array", Rpc::TYPE_ERROR);
    }

    if (size < 0) {
        throw Rpc::Exception("range error: array index is negative", Rpc::INDEX_RANGE_ERROR);
    } else if ( _value.asArray->size() < static_cast<ValueArray::size_type>(size) ) { // cast to uint is safe since we checked it above.
        throw Rpc::Exception("range error: array index too large", Rpc::INDEX_RANGE_ERROR);
    }
}

void Value::assertArray(int size)
{
    if (_type == TypeInvalid) {
        _type = TypeArray;
        _value.asArray = new ValueArray(size);
    } else if (_type == TypeArray) {
        if (size < 0) {
            throw Rpc::Exception("range error: array index is negative", Rpc::INDEX_RANGE_ERROR);
        } else if ( _value.asArray->size() < static_cast<ValueArray::size_type>(size) ) { // cast to uint is safe since we checked it above.
            _value.asArray->resize(size);
        }
    } else {
        throw Rpc::Exception("type error: expected an array", Rpc::TYPE_ERROR);
    }
}

void Value::assertStruct()
{
    if (_type == TypeInvalid) {
        _type = TypeStruct;
        _value.asStruct = new ValueStruct();
    } else if (_type != TypeStruct) {
        throw Rpc::Exception("type error: expected a struct", Rpc::TYPE_ERROR);
    }
}

void Value::assertStruct() const
{
    if (_type != TypeStruct) {
        throw Rpc::Exception("type error: expected a struct", Rpc::TYPE_ERROR);
    }
}

Value::Value(Value const& rhs)
    : _type(rhs._type)
{
    switch (_type) {
    case TypeNil:      _value.asBinary = 0; break;
    case TypeBoolean:  _value.asBool = rhs._value.asBool; break;
    case TypeInt:      _value.asInt = rhs._value.asInt; break;
    case TypeDouble:   _value.asDouble = rhs._value.asDouble; break;
    case TypeDateTime: _value.asTime = new struct tm(*rhs._value.asTime); break;
    case TypeString:   _value.asString = new std::string(*rhs._value.asString); break;
    case TypeBase64:   _value.asBinary = new BinaryData(*rhs._value.asBinary); break;
    case TypeArray:    _value.asArray = new ValueArray(*rhs._value.asArray); break;
    case TypeStruct:   _value.asStruct = new ValueStruct(*rhs._value.asStruct); break;
    default:           _value.asBinary = 0; break;
    }
}

// Operators
Value& Value::operator=(Value const& rhs)
{
    Value(rhs).swap(*this);
    return *this;
}

// Predicate for tm equality
static bool tmEq(struct tm const& t1, struct tm const& t2) {
    return t1.tm_sec == t2.tm_sec && t1.tm_min == t2.tm_min &&
    t1.tm_hour == t2.tm_hour && t1.tm_mday == t1.tm_mday &&
    t1.tm_mon == t2.tm_mon && t1.tm_year == t2.tm_year;
}

bool Value::operator==(Value const& other) const
{
    if (_type != other._type) {
        return false;
    }

    switch (_type)
    {
    case TypeNil:      return true;
    case TypeBoolean:  return ( !_value.asBool && !other._value.asBool)
                                || ( _value.asBool && other._value.asBool);
    case TypeInt:      return _value.asInt == other._value.asInt;
    case TypeDouble:   return _value.asDouble == other._value.asDouble;
    case TypeDateTime: return tmEq(*_value.asTime, *other._value.asTime);
    case TypeString:   return *_value.asString == *other._value.asString;
    case TypeBase64:   return *_value.asBinary == *other._value.asBinary;
    case TypeArray:    return *_value.asArray == *other._value.asArray;

        // The map<>::operator== requires the definition of value< for kcc
    case TypeStruct:   //return *_value.asStruct == *other._value.asStruct;
        {
            if (_value.asStruct->size() != other._value.asStruct->size()) {
                return false;
            }

            ValueStruct::const_iterator it1=_value.asStruct->begin();
            ValueStruct::const_iterator it2=other._value.asStruct->begin();
            while (it1 != _value.asStruct->end()) {
                const Value& v1 = it1->second;
                const Value& v2 = it2->second;
                if (!(v1 == v2)) {
                    return false;
                }
                it1++;
                it2++;
            }
            return true;
        }
    default: break;
    }
    return true;    // Both invalid values ...
}

bool Value::operator!=(Value const& other) const
{
    return !(*this == other);
}

// Works for strings, binary data, arrays, and structs.
int Value::size() const
{
    switch (_type)
    {
    case TypeString: return int(_value.asString->size());
    case TypeBase64: return int(_value.asBinary->size());
    case TypeArray:  return int(_value.asArray->size());
    case TypeStruct: return int(_value.asStruct->size());
    default: break;
    }

    throw Rpc::Exception("type error", Rpc::TYPE_ERROR);
}

// Checks for existence of struct member
bool Value::hasMember(const std::string& name) const
{
    return _type == TypeStruct && _value.asStruct->find(name) != _value.asStruct->end();
}

bool Value::hasMember(const char* name) const
{
    return _type == TypeStruct && _value.asStruct->find(name) != _value.asStruct->end();
}

Value::ValueStruct::const_iterator Value::getStructMembersBegin() const
{
    assertStruct();
    return _value.asStruct->begin();
}

Value::ValueStruct::const_iterator Value::getStructMembersEnd() const
{
    assertStruct();
    return _value.asStruct->end();
}

// Set the value from xml. The chars at *offset into valueXml
// should be the start of a <value> tag. Destroys any existing value.
bool Value::fromXml(std::string const& valueXml, std::size_t* offset)
{
    std::size_t savedOffset = *offset;

    invalidate();
    if ( ! Util::nextTagIs(VALUE_TAG, valueXml, offset)) {
        return false;       // Not a value, offset not updated
    }

    std::size_t afterValueOffset = *offset;
    std::string typeTag = Util::getNextTag(valueXml, offset);
    bool result = false;
    if (typeTag == NIL_TAG || typeTag == NIL_TAG_FULL)
        result = nilFromXml(valueXml, offset);
    if (typeTag == BOOLEAN_TAG)
        result = boolFromXml(valueXml, offset);
    else if (typeTag == I4_TAG || typeTag == INT_TAG)
        result = intFromXml(valueXml, offset);
    else if (typeTag == DOUBLE_TAG)
        result = doubleFromXml(valueXml, offset);
    else if (typeTag.empty() || typeTag == STRING_TAG)
        result = stringFromXml(valueXml, offset);
    else if (typeTag == DATETIME_TAG)
        result = timeFromXml(valueXml, offset);
    else if (typeTag == BASE64_TAG)
        result = binaryFromXml(valueXml, offset);
    else if (typeTag == ARRAY_TAG)
        result = arrayFromXml(valueXml, offset);
    else if (typeTag == STRUCT_TAG)
        result = structFromXml(valueXml, offset);
    // Watch for empty/blank strings with no <string>tag
    else if (typeTag == VALUE_ETAG) {
        *offset = afterValueOffset;   // back up & try again
        result = stringFromXml(valueXml, offset);
    }

    if (result) { // Skip over the </value> tag
        Util::findTag(VALUE_ETAG, valueXml, offset);
    } else { // Unrecognized tag after <value>
        *offset = savedOffset;
    }
    return result;
}

// Encode the Value in xml
std::string Value::toXml() const
{
    switch (_type)
    {
    case TypeNil:      return nilToXml();
    case TypeBoolean:  return boolToXml();
    case TypeInt:      return intToXml();
    case TypeDouble:   return doubleToXml();
    case TypeString:   return stringToXml();
    case TypeDateTime: return timeToXml();
    case TypeBase64:   return binaryToXml();
    case TypeArray:    return arrayToXml();
    case TypeStruct:   return structToXml();
    default: break;
    }
    return std::string();   // Invalid value
}

// Nil
bool Value::nilFromXml(std::string const& valueXml, std::size_t* /*offset*/)
{
    _type = TypeNil;
    _value.asBinary = 0;
    return true;
}

std::string Value::nilToXml() const
{
    std::string xml = VALUE_TAG;
    xml += NIL_TAG_FULL;
    xml += VALUE_ETAG;
    return xml;
}

// Boolean
bool Value::boolFromXml(std::string const& valueXml, std::size_t* offset)
{
    const char* valueStart = valueXml.c_str() + *offset;
    char* valueEnd;
    long ivalue = strtol(valueStart, &valueEnd, 10);
    if (valueEnd == valueStart || (ivalue != 0 && ivalue != 1)) {
        return false;
    }
    _type = TypeBoolean;
    _value.asBool = (ivalue == 1);
    *offset += valueEnd - valueStart;
    return true;
}

std::string Value::boolToXml() const
{
    std::string xml = VALUE_TAG;
    xml += BOOLEAN_TAG;
    xml += (_value.asBool ? "1" : "0");
    xml += BOOLEAN_ETAG;
    xml += VALUE_ETAG;
    return xml;
}

// Int
bool Value::intFromXml(std::string const& valueXml, std::size_t* offset)
{
    const char* valueStart = valueXml.c_str() + *offset;
    char* valueEnd;

    long ivalue = strtol(valueStart, &valueEnd, 10);
    if (valueEnd == valueStart) {
        return false;
    }

    _type = TypeInt;
    _value.asInt = int(ivalue); // TODO: ensure that integer overflow impossible
    *offset += valueEnd - valueStart;
    return true;
}

std::string Value::intToXml() const
{
    char buf[30];
    snprintf(buf, sizeof(buf)-1, "%d", _value.asInt);
    buf[sizeof(buf)-1] = 0;
    std::string xml = VALUE_TAG;
    xml += I4_TAG;
    xml += buf;
    xml += I4_ETAG;
    xml += VALUE_ETAG;
    return xml;
}

// Double
bool Value::doubleFromXml(std::string const& valueXml, std::size_t* offset)
{
    const char* valueStart = valueXml.c_str() + *offset;
    char* valueEnd;
    double dvalue = strtod(valueStart, &valueEnd);
    if (valueEnd == valueStart) {
        return false;
    }

    _type = TypeDouble;
    _value.asDouble = dvalue;
    *offset += valueEnd - valueStart;
    return true;
}

std::string Value::doubleToXml() const
{
    char buf[100];
    snprintf(buf, sizeof(buf)-1, kDoubleFormat.c_str(), _value.asDouble);
    buf[sizeof(buf)-1] = 0;

    std::string xml = VALUE_TAG;
    xml += DOUBLE_TAG;
    xml += buf;
    xml += DOUBLE_ETAG;
    xml += VALUE_ETAG;
    return xml;
}

// String
bool Value::stringFromXml(std::string const& valueXml, std::size_t* offset)
{
    size_t valueEnd = valueXml.find('<', *offset);
    if (valueEnd == std::string::npos) {
        return false;     // No end tag;
    }

    _type = TypeString;
    _value.asString = new std::string(Util::xmlDecode(valueXml.substr(*offset, valueEnd - *offset)));
    *offset += _value.asString->length();
    return true;
}

std::string Value::stringToXml() const
{
    std::string xml = VALUE_TAG;
    //xml += STRING_TAG; optional
    xml += Util::xmlEncode(*_value.asString);
    //xml += STRING_ETAG;
    xml += VALUE_ETAG;
    return xml;
}

// DateTime (stored as a struct tm)
bool Value::timeFromXml(std::string const& valueXml, std::size_t* offset)
{
    size_t valueEnd = valueXml.find('<', *offset);
    if (valueEnd == std::string::npos) {
        return false;     // No end tag;
    }

    std::string stime = valueXml.substr(*offset, valueEnd-*offset);

    struct tm t;
    if (sscanf_s(stime.c_str(),"%4d%2d%2dT%2d:%2d:%2d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec) != 6) {
        return false;
    }

    t.tm_isdst = -1;
    _type = TypeDateTime;
    _value.asTime = new struct tm(t);
    *offset += stime.length();
    return true;
}

std::string Value::timeToXml() const
{
    struct tm* t = _value.asTime;
    char buf[20];
    snprintf(buf, sizeof(buf)-1, "%4d%02d%02dT%02d:%02d:%02d",
    t->tm_year,t->tm_mon,t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec);
    buf[sizeof(buf)-1] = 0;

    std::string xml = VALUE_TAG;
    xml += DATETIME_TAG;
    xml += buf;
    xml += DATETIME_ETAG;
    xml += VALUE_ETAG;
    return xml;
}

// Base64
bool Value::binaryFromXml(std::string const& valueXml, std::size_t* offset)
{
    size_t valueEnd = valueXml.find('<', *offset);
    if (valueEnd == std::string::npos) {
        return false;     // No end tag;
    }

    _type = TypeBase64;
    std::string asString = valueXml.substr(*offset, valueEnd-*offset);
    _value.asBinary = new BinaryData();
    // check whether base64 encodings can contain chars xml encodes...

    using namespace Base64Utils;
    // convert from base64 to binary
    int iostatus = 0;
    base64<char> decoder;
    std::back_insert_iterator<BinaryData> ins = std::back_inserter(*(_value.asBinary));
    decoder.get(asString.begin(), asString.end(), ins, iostatus);

    *offset += asString.length();
    return true;
}

std::string Value::binaryToXml() const
{
    using namespace Base64Utils;
    // convert to base64
    std::vector<char> base64data;
    int iostatus = 0;
    base64<char> encoder;
    std::back_insert_iterator<std::vector<char> > ins = std::back_inserter(base64data);
    encoder.put(_value.asBinary->begin(), _value.asBinary->end(), ins, iostatus, base64<>::crlf());

    // Wrap with xml
    std::string xml = VALUE_TAG;
    xml += BASE64_TAG;
    xml.append(base64data.begin(), base64data.end());
    xml += BASE64_ETAG;
    xml += VALUE_ETAG;
    return xml;
}

// Array
bool Value::arrayFromXml(std::string const& valueXml, std::size_t* offset)
{
    if ( ! Util::nextTagIs(DATA_TAG, valueXml, offset)) {
        return false;
    }

    _type = TypeArray;
    _value.asArray = new ValueArray;
    Value v;
    while ( v.fromXml(valueXml, offset) ) {
        _value.asArray->push_back(v);       // copy...
    }

    // Skip the trailing </data>
    Util::nextTagIs(DATA_ETAG, valueXml, offset);
    return true;
}

// In general, its preferable to generate the xml of each element of the
// array as it is needed rather than glomming up one big string.
std::string Value::arrayToXml() const
{
    std::string xml = VALUE_TAG;
    xml += ARRAY_TAG;
    xml += DATA_TAG;

    ValueArray::size_type size = _value.asArray->size();
    for (ValueArray::size_type i = 0; i < size; ++i) {
        xml += _value.asArray->at(i).toXml();
    }

    xml += DATA_ETAG;
    xml += ARRAY_ETAG;
    xml += VALUE_ETAG;
    return xml;
}

// Struct
bool Value::structFromXml(std::string const& valueXml, std::size_t* offset)
{
    _type = TypeStruct;
    _value.asStruct = new ValueStruct;

    while ( Util::nextTagIs(MEMBER_TAG, valueXml, offset) ) {
        // name
        const std::string name = XmlRpc::Util::parseTag(NAME_TAG, valueXml, offset);
        // value
        Value val(valueXml, offset);
        if ( !val.valid() ) {
            invalidate();
            return false;
        }
        _value.asStruct->insert( std::make_pair(name, val) ); ;

        Util::nextTagIs(MEMBER_ETAG, valueXml, offset);
    }
    return true;
}

// In general, its preferable to generate the xml of each element
// as it is needed rather than glomming up one big string.
std::string Value::structToXml() const
{
    std::string xml = VALUE_TAG;
    xml += STRUCT_TAG;

    ValueStruct::const_iterator it = _value.asStruct->begin(),
                                end = _value.asStruct->end();
    for (; it != end; ++it) {
        xml += MEMBER_TAG;
        xml += NAME_TAG;
        xml += Util::xmlEncode(it->first);
        xml += NAME_ETAG;
        xml += it->second.toXml();
        xml += MEMBER_ETAG;
    }

    xml += STRUCT_ETAG;
    xml += VALUE_ETAG;
    return xml;
}

// Write the value without xml encoding it
std::ostream& Value::write(std::ostream& os) const {
    switch (_type)
    {
    default:           break;
    case TypeNil:      break;
    case TypeBoolean:  os << _value.asBool; break;
    case TypeInt:      os << _value.asInt; break;
    case TypeDouble:   os << _value.asDouble; break;
    case TypeString:   os << *_value.asString; break;
    case TypeDateTime:
        {
            struct tm* t = _value.asTime;
            char buf[20];
            snprintf(buf, sizeof(buf) - 1, "%4d%02d%02dT%02d:%02d:%02d",
            t->tm_year,t->tm_mon,t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec);
            buf[sizeof(buf) - 1] = 0;
            os << buf;
            break;
        }
    case TypeBase64:
        {
            using namespace Base64Utils;
            int iostatus = 0;
            std::ostreambuf_iterator<char> out(os);
            base64<char> encoder;
            encoder.put(_value.asBinary->begin(), _value.asBinary->end(), out, iostatus, base64<>::crlf());
            break;
        }
    case TypeArray:
        {
            os << '{';
            const ValueArray::size_type array_size = _value.asArray->size();
            for (ValueArray::size_type i = 0; i < array_size; ++i) {
                if (i != 0) {
                    os << ',';
                }
                _value.asArray->at(i).write(os);
            }
            os << '}';
            break;
        }
    case TypeStruct:
        {
            os << '[';
            ValueStruct::const_iterator it,
                                        begin = _value.asStruct->begin(),
                                        end = _value.asStruct->end();
            for (it = _value.asStruct->begin(); it != end; ++it) {
                if (it != begin) {
                    os << ',';
                }
                os << it->first << ':';
                it->second.write(os);
            }
            os << ']';
            break;
        }

    }

    return os;
}

// ostream
std::ostream& operator<<(std::ostream& os, Value& v)
{
    // If you want to output in xml format:
    //return os << v.toXml();
    return v.write(os);
}

} // namespace XmlRpc
