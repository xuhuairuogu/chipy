#pragma once

#include <memory>
#include <stdint.h>
#include <string>
#include "json/json.h"
#include "Object.h"

namespace chipy
{

enum class ValueType
{
    Bool,
    String,
    Integer,
    Float,
    DictItems,
    CppObject,
    Attribute,
    Builtin,
    List,
    Dictionary,
    Iterator,
    Tuple,
    Alias,
    Module,
    Function
};

class Value;
typedef std::shared_ptr<Value> ValuePtr;

template<typename T>
std::shared_ptr<T> wrap_value(T *val)
{
    return std::shared_ptr<T>{val};
}

template<typename T, class... Args>
std::shared_ptr<T> make_value(MemoryManager &mem, Args&&... args)
{
    auto val = new (mem) T(mem, std::forward<Args>(args)...);
    return wrap_value(val);
}

class Value : public Object
{
public:
    virtual ValueType type() const = 0;

    ValuePtr duplicate()
    {
        return duplicate(memory_manager());
    }

    virtual ValuePtr duplicate(MemoryManager &mem) = 0;

    virtual bool is_generator() const
    {
        return false;
    }

    virtual bool can_iterate() const
    {
        return false;
    }

    virtual bool is_callable() const
    {
        return false;
    }

    virtual bool bool_test() const
    {
        return true;
    }

    virtual ~Value() {}

protected:
    Value(MemoryManager &mem) : Object(mem)
        {}
};


bool operator==(const Value &v1, const Value& v2);

template<typename value_type, ValueType value_type_val>
class PlainValue : public Value
{
public:
    PlainValue(MemoryManager &mem, value_type val)
        : Value(mem), m_value(val)
    {}

    ValueType type() const override
    {
        return value_type_val;
    }

    const value_type& get() const
    {
        return m_value;
    }

    void set(const value_type &v)
    {
        m_value = v;
    }

protected:
    value_type m_value;
};

class Alias : public Value
{
public:
    Alias(MemoryManager &mem, const std::string& name, const std::string &as_name)
        : Value(mem), m_name(name), m_as_name(as_name)
    {}

    ValueType type() const override
    {
        return ValueType::Alias;
    }

    ValuePtr duplicate(MemoryManager &memory_manager) override
    {
        return wrap_value(new (memory_manager) Alias(memory_manager, name(), as_name()));
    }

    const std::string name() const { return m_name; }
    const std::string as_name() const { return m_as_name; }

private:
    const std::string m_name, m_as_name;
};

class BoolVal : public PlainValue<bool, ValueType::Bool>
{
public:
    BoolVal(MemoryManager &mem, bool val)
        : PlainValue(mem, val) {}

    ValuePtr duplicate(MemoryManager &mem) override
    { return wrap_value(new (mem) BoolVal(mem, m_value)); }

    bool bool_test() const override { return m_value; }
};

class StringVal : public PlainValue<std::string, ValueType::String>
{
public:
    StringVal(MemoryManager &mem, const std::string &val)
        : PlainValue(mem, val) {}

    ValuePtr duplicate(MemoryManager &mem) override
    { return wrap_value(new (mem) StringVal(mem, m_value)); }
};

class FloatVal : public PlainValue<double, ValueType::Float>
{
public:
    FloatVal(MemoryManager &mem, const double &val) 
        : PlainValue(mem, val) {}

    ValuePtr duplicate(MemoryManager &mem) override
    { return wrap_value(new (mem) FloatVal(mem, m_value)); }
};

class IntVal : public PlainValue<int32_t, ValueType::Integer>
{
public:
    IntVal(MemoryManager &mem, const int32_t &val)
        : PlainValue(mem, val) {}

    ValuePtr duplicate(MemoryManager &mem) override
    { return wrap_value(new (mem) IntVal(mem, m_value)); }

    bool bool_test() const override
    { return m_value != 0; }
};

typedef std::shared_ptr<IntVal> IntValPtr;
typedef std::shared_ptr<StringVal> StringValPtr;
typedef std::shared_ptr<FloatVal> FloatValPtr;
typedef std::shared_ptr<BoolVal> BoolValPtr;

class value_exception
{
    std::string m_what;

public:
    value_exception(const std::string &what)
        : m_what(what) {}

    const std::string& what() const
    {
        return m_what;
    }
};

template<typename T>
std::shared_ptr<T> value_cast(ValuePtr val)
{
    auto res = std::dynamic_pointer_cast<T>(val);
    if(res == nullptr)
        throw value_exception("Invalid value cast");

    return res;
}


inline bool operator>(const Value &first, const Value &second)
{
    if(first.type() == ValueType::Integer && second.type() == ValueType::Integer)
    {
        return dynamic_cast<const IntVal&>(first).get() > dynamic_cast<const IntVal&>(second).get();
    }
    else
        return false;
}

inline bool operator>=(const Value &first, const Value &second)
{
    if(first.type() == ValueType::Integer && second.type() == ValueType::Integer)
    {
        return dynamic_cast<const IntVal&>(first).get() >= dynamic_cast<const IntVal&>(second).get();
    }
    else
        return false;
}

inline bool operator==(const Value &first, const Value &second)
{
    if(first.type() == ValueType::String && second.type() == ValueType::String)
    {
        return dynamic_cast<const StringVal&>(first).get() == dynamic_cast<const StringVal&>(second).get();
    }
    else if(first.type() == ValueType::Integer && second.type() == ValueType::Integer)
    {
        return dynamic_cast<const IntVal&>(first).get() == dynamic_cast<const IntVal&>(second).get();
    }
    else
        return false;
}

/**
 * @brief Serialize a Value to a BitStream
 *
 * @note This currently only supports plain values
 */
inline BitStream& operator<<(BitStream &bs, ValuePtr val)
{
    switch(val->type())
    {
    case ValueType::Integer:
    {
        auto ival = value_cast<IntVal>(val);
        bs << ValueType::Integer << ival->get();
        break;
    }
    case ValueType::Bool:
    {
        auto bval = value_cast<BoolVal>(val);
        bs << ValueType::Bool << bval->get();
        break;
    }
    default:
        throw value_exception("Cannot serialize this type of value!");
    }

    return bs;
}

/**
 * @brief Read a value from a bitstream
 *
 * @param bs
 *      The BitStream containg the value
 * @param mem
 *      The value will be initialized in mem's context
 */
inline ValuePtr read_value(BitStream &bs, MemoryManager &mem)
{
    ValueType type;
    bs >> type;

    switch(type)
    {
    case ValueType::Integer:
    {
        int32_t i = 0;
        bs >> i;
        return mem.create_integer(i);
    }
    case ValueType::Bool:
    {
        bool b = false;
        bs >> b;
        return mem.create_boolean(b);
    }
    default:
        throw value_exception("Cannot serialize this type of value!");
    }
}

}
