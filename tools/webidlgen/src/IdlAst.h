// WebIDL AST for the TinyBrowser WebIDL compiler.
//
// The AST mirrors the subset of the W3C Web IDL grammar that the generator
// understands.  Everything that the generator cannot represent is still
// parsed (so that the parser never loses sync with the input) but is marked
// with a dedicated node kind and skipped during code generation.
#pragma once

#include <string>
#include <vector>

namespace webidlgen {

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

enum class TypeKind {
    Void,
    Boolean,
    Integer,          // byte / short / long / long long
    UnsignedInteger,  // unsigned byte / short / long / long long
    BigInt,
    Float,
    Double,
    UnrestrictedFloat,
    UnrestrictedDouble,
    String,      // DOMString / ByteString / USVString
    Object,      // object
    Symbol,
    Any,
    Sequence,
    FrozenArray,
    ObservableArray,
    Promise,
    Union,
    Interface,
    Enum,
    Dictionary,
    Callback,
    Unknown,
};

struct Type {
    TypeKind kind = TypeKind::Unknown;
    // Builtin type name ("DOMString"), or the referenced definition name
    // ("Node") for interface / enum / dictionary / callback references.
    std::string name;
    bool nullable = false;
    // sequence<T> / Promise<T> -> single element, union -> alternatives.
    std::vector<Type> params;
    // T[] is modelled as isArray == true with params[0] holding the element.
    bool isArray = false;

    bool isSequenceLike() const {
        return kind == TypeKind::Sequence || kind == TypeKind::FrozenArray ||
               kind == TypeKind::ObservableArray || isArray;
    }
};

// ---------------------------------------------------------------------------
// Extended attributes
// ---------------------------------------------------------------------------

struct ExtendedAttribute {
    std::string name;
    bool hasValue = false;
    std::string value;              // Name=Value
    std::vector<std::string> list;  // Name=(a, b)
};

// ---------------------------------------------------------------------------
// Members
// ---------------------------------------------------------------------------

enum class MemberKind {
    Constant,
    Attribute,
    Operation,
    Constructor,
    Stringifier,
    Iterable,
    Maplike,
    Setlike,
    Getter,
    Setter,
    Deleter,
};

struct Argument {
    Type type;
    std::string name;
    bool optional = false;
    bool variadic = false;
    bool hasDefault = false;
    std::string defaultValue;  // raw literal as written in the IDL
};

struct Member {
    MemberKind kind = MemberKind::Operation;
    Type type;                 // attribute type / constant type / return type
    std::string name;
    bool isStatic = false;
    bool isReadonly = false;
    bool isInherit = false;
    bool isAsync = false;
    bool isPromise = false;
    std::vector<Argument> arguments;
    std::string constantValue;
    std::vector<ExtendedAttribute> extended;

    bool isAttributeLike() const {
        return kind == MemberKind::Attribute || kind == MemberKind::Getter ||
               kind == MemberKind::Setter || kind == MemberKind::Deleter;
    }
};

// ---------------------------------------------------------------------------
// Definitions
// ---------------------------------------------------------------------------

struct Interface {
    std::string name;
    std::string parent;
    bool partial = false;
    bool isMixin = false;
    std::vector<Member> members;
    std::vector<ExtendedAttribute> extended;
    std::string sourceFile;
};

struct Dictionary {
    std::string name;
    std::vector<Member> members;
    std::vector<ExtendedAttribute> extended;
    std::string sourceFile;
};

struct Enum {
    std::string name;
    std::vector<std::string> values;
    std::string sourceFile;
};

struct Callback {
    std::string name;
    Type returnType;
    std::vector<Argument> arguments;
    std::string sourceFile;
};

struct Typedef {
    std::string name;
    Type type;
    std::string sourceFile;
};

// ---------------------------------------------------------------------------
// Database
// ---------------------------------------------------------------------------

struct Database {
    std::vector<Interface> interfaces;
    std::vector<Dictionary> dictionaries;
    std::vector<Enum> enums;
    std::vector<Callback> callbacks;
    std::vector<Typedef> typedefs;
    std::vector<std::string> diagnostics;

    const Interface* findInterface(const std::string& name) const;
    const Enum* findEnum(const std::string& name) const;
    const Dictionary* findDictionary(const std::string& name) const;
    const Typedef* findTypedef(const std::string& name) const;
    const Callback* findCallback(const std::string& name) const;

    // Depth of the interface in the inheritance chain (0 for root interfaces).
    int inheritanceDepth(const std::string& name) const;
    // Direct subclasses of `name`, in declaration order.
    std::vector<const Interface*> subclassesOf(const std::string& name) const;
};

}  // namespace webidlgen
