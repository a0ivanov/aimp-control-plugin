#ifndef _Value_H_
#define _Value_H_
//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
//

#ifndef MAKEDEPEND
# include <map>
# include <string>
# include <vector>
# include <time.h>
#endif

namespace XmlRpc
{

// RPC method arguments and results are represented by Values
// should probably refcount them...
class Value
{
public:
    enum Type {
      TypeInvalid,
      TypeNil,
      TypeBoolean,
      TypeInt,
      TypeDouble,
      TypeString,
      TypeDateTime,
      TypeBase64,
      TypeArray,
      TypeStruct
    };

    // Non-primitive types
    typedef std::vector<char> BinaryData;
    typedef std::vector<Value> ValueArray;
    typedef std::map<std::string, Value> ValueStruct;
    struct Nil {};

    //! Constructors
    Value() : _type(TypeInvalid) { _value.asBinary = 0; }
    Value(const Nil& ) : _type(TypeNil) { _value.asBinary = 0; }
    Value(bool value) : _type(TypeBoolean) { _value.asBool = value; }
    Value(int value)  : _type(TypeInt) { _value.asInt = value; }
    Value(unsigned int value)  : _type(TypeInt) { _value.asInt = value; }
    Value(double value)  : _type(TypeDouble) { _value.asDouble = value; }

    Value(std::string const& value) : _type(TypeString)
    { _value.asString = new std::string(value); }

    Value(const char* value)  : _type(TypeString)
    { _value.asString = new std::string(value); }

    Value(struct tm* value)  : _type(TypeDateTime)
    { _value.asTime = new struct tm(*value); }

    Value(void* value, int nBytes)  : _type(TypeBase64)
    { _value.asBinary = new BinaryData((char*)value, ((char*)value)+nBytes); }

    Value(ValueStruct const& value) : _type(TypeStruct)
    { _value.asStruct = new ValueStruct(value); }

    //! Construct from xml, beginning at *offset chars into the string, updates offset
    Value(std::string const& xml, std::size_t* offset) : _type(TypeInvalid)
    { if ( !fromXml(xml, offset) ) _type = TypeInvalid; }

    //! Copy
    Value(Value const& rhs);

    //! Destructor (make virtual if you want to subclass)
    /*virtual*/ ~Value() { invalidate(); }

    //! Erase the current value
    void clear() { invalidate(); }

    // Operators
    Value& operator=(Value const& rhs);
    Value& operator=(const Nil& rhs) { return operator=(Value(rhs)); }
    Value& operator=(bool rhs) { return operator=(Value(rhs)); }
    Value& operator=(int rhs) { return operator=(Value(rhs)); }
    Value& operator=(unsigned int rhs) { return operator=(Value(rhs)); }
    Value& operator=(double rhs) { return operator=(Value(rhs)); }
    Value& operator=(const char* rhs) { return operator=(Value(rhs)); }
    Value& operator=(const ValueStruct& rhs) { return operator=(Value(rhs)); }

    bool operator==(Value const& other) const;
    bool operator!=(Value const& other) const;


    operator bool&()          { assertTypeOrInvalid(TypeBoolean); return _value.asBool; }
    operator bool const&() const { assertType(TypeBoolean); return _value.asBool; }
    operator int&()           { assertTypeOrInvalid(TypeInt); return _value.asInt; }
    operator int const&() const { assertType(TypeInt); return _value.asInt; }
    operator double&()        { assertTypeOrInvalid(TypeDouble); return _value.asDouble; }
    operator double const&() const { assertType(TypeDouble); return _value.asDouble; }
    operator std::string&()   { assertTypeOrInvalid(TypeString); return *_value.asString; }
    operator std::string const&() const { assertType(TypeString); return *_value.asString; }
    operator BinaryData&()    { assertTypeOrInvalid(TypeBase64); return *_value.asBinary; }
    operator BinaryData const&() const { assertType(TypeBase64); return *_value.asBinary; }
    operator struct tm&()     { assertTypeOrInvalid(TypeDateTime); return *_value.asTime; }
    operator struct tm const&() const { assertType(TypeDateTime); return *_value.asTime; }

    Value& operator[](int i) { assertArray(i+1); return _value.asArray->at(i); }
    Value const& operator[](int i) const;
    Value& operator[](std::string const& k) { assertStruct(); return (*_value.asStruct)[k]; }
    Value const& operator[](std::string const& k) const;
    Value& operator[](const char* k) { assertStruct(); return (*_value.asStruct)[k]; }
    Value const& operator[](const char* k) const;

    // Accessors
    //! Return true if the value has been set to something.
    bool valid() const { return _type != TypeInvalid; }

    //! Return the type of the value stored. \see Type.
    Type getType() const { return _type; }

    //! Return the size for string, base64, array, and struct values.
    int size() const;

    //! Specify the size for array values. Array values will grow beyond this size if needed.
    void setSize(int size) { assertArray(size); }

    //! Check for the existence of a struct member by name.
    bool hasMember(const std::string& name) const;
    //! Check for the existence of a struct member by name. Added for perfo
    bool hasMember(const char* name) const;

    ValueStruct::const_iterator getStructMembersBegin() const;
    ValueStruct::const_iterator getStructMembersEnd() const;

    //! Decode xml. Destroys any existing value.
    bool fromXml(std::string const& valueXml, std::size_t* offset);

    //! Encode the Value in xml
    std::string toXml() const;

    //! Write the value (no xml encoding)
    std::ostream& write(std::ostream& os) const;

    void swap(Value& rhs);

protected:

    // Clean up
    void invalidate();

    // Type checking
    void assertTypeOrInvalid(Type t);
    void assertType(Type t) const;
    void assertArray(int size) const;
    void assertArray(int size);
    void assertStruct();
    void assertStruct() const;

    // XML decoding
    bool nilFromXml(std::string const& valueXml, std::size_t* offset);
    bool boolFromXml(std::string const& valueXml, std::size_t* offset);
    bool intFromXml(std::string const& valueXml, std::size_t* offset);
    bool doubleFromXml(std::string const& valueXml, std::size_t* offset);
    bool stringFromXml(std::string const& valueXml, std::size_t* offset);
    bool timeFromXml(std::string const& valueXml, std::size_t* offset);
    bool binaryFromXml(std::string const& valueXml, std::size_t* offset);
    bool arrayFromXml(std::string const& valueXml, std::size_t* offset);
    bool structFromXml(std::string const& valueXml, std::size_t* offset);

    // XML encoding
    std::string nilToXml() const;
    std::string boolToXml() const;
    std::string intToXml() const;
    std::string doubleToXml() const;
    std::string stringToXml() const;
    std::string timeToXml() const;
    std::string binaryToXml() const;
    std::string arrayToXml() const;
    std::string structToXml() const;

    // Type tag and values
    Type _type;

    // At some point I will split off Arrays and Structs into
    // separate ref-counted objects for more efficient copying.
    union {
      bool          asBool;
      int           asInt;
      double        asDouble;
      struct tm*    asTime;
      std::string*  asString;
      BinaryData*   asBinary;
      ValueArray*   asArray;
      ValueStruct*  asStruct;
    } _value;

};

std::ostream& operator<<(std::ostream& os, Value& v);

} // namespace XmlRpc

#endif // #ifndef _Value_H_
