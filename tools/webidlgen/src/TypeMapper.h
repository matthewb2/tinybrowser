// Maps WebIDL types onto their C++ representation and the V8 conversion
// strategy that the generated bindings use.
#pragma once

#include <string>
#include <vector>

#include "IdlAst.h"

namespace webidlgen {

struct TypeInfo {
    enum class Kind {
        Void,
        Bool,
        Int8,
        Int16,
        Int32,
        Int64,
        UInt8,
        UInt16,
        UInt32,
        UInt64,
        Double,
        String,
        Object,
        Value,
        BigInt,
        Interface,
        Sequence,
    };

    // Flat description of the element type of a sequence.  It is kept
    // separate from TypeInfo so that the description stays non recursive.
    struct Element {
        Kind kind = Kind::Value;
        bool nullable = false;
        std::string cppType;
        std::string conversionSuffix;
        std::string interfaceName;
    };

    Kind kind = Kind::Value;
    bool nullable = false;
    bool isSequence = false;
    // C++ type used in the generated native interface.
    std::string cppType;
    // Suffix used to build the ToV8*/FromV8* helper names.
    std::string conversionSuffix;
    // Interface name for Kind::Interface.
    std::string interfaceName;
    // Element type for Kind::Sequence.
    Element element;

    // The V8 value type produced by the conversion helper.
    std::string v8Type() const;
};

class TypeMapper {
public:
    explicit TypeMapper(const Database& database) : m_db(database) {}

    TypeInfo map(const Type& type) const;

    // True when the type maps onto a reference to a WebIDL interface.
    bool isInterfaceType(const Type& type) const;
    // True when the type is (or resolves to) a string type.
    bool isStringType(const Type& type) const;

private:
    TypeInfo mapResolved(Type type) const;

    const Database& m_db;
};

}  // namespace webidlgen
