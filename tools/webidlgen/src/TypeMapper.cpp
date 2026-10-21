#include "TypeMapper.h"

#include <algorithm>

namespace webidlgen {
namespace {

std::string capitalize(const std::string& s) {
    if (s.empty()) return s;
    std::string out = s;
    out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    return out;
}

}  // namespace

std::string TypeInfo::v8Type() const {
    switch (kind) {
        case Kind::Void:
            return "v8::Local<v8::Value>";
        case Kind::Bool:
        case Kind::Int8:
        case Kind::Int16:
        case Kind::Int32:
        case Kind::Int64:
        case Kind::UInt8:
        case Kind::UInt16:
        case Kind::UInt32:
        case Kind::UInt64:
        case Kind::Double:
        case Kind::BigInt:
            return "v8::Local<v8::Value>";
        case Kind::String:
            return "v8::Local<v8::String>";
        case Kind::Object:
            return "v8::Local<v8::Object>";
        case Kind::Interface:
            return "v8::Local<v8::Object>";
        case Kind::Value:
            return "v8::Local<v8::Value>";
        case Kind::Sequence:
            return "v8::Local<v8::Array>";
    }
    return "v8::Local<v8::Value>";
}

bool TypeMapper::isInterfaceType(const Type& type) const {
    Type resolved = type;
    // Resolve typedefs.
    for (int i = 0; i < 8; ++i) {
        if (resolved.kind != TypeKind::Interface) break;
        const Typedef* td = m_db.findTypedef(resolved.name);
        if (!td) break;
        resolved = td->type;
    }
    return resolved.kind == TypeKind::Interface &&
           m_db.findInterface(resolved.name) != nullptr;
}

bool TypeMapper::isStringType(const Type& type) const {
    Type resolved = type;
    for (int i = 0; i < 8; ++i) {
        if (resolved.kind == TypeKind::String) return true;
        if (resolved.kind != TypeKind::Interface) break;
        const Typedef* td = m_db.findTypedef(resolved.name);
        if (!td) break;
        resolved = td->type;
    }
    return resolved.kind == TypeKind::String;
}

TypeInfo TypeMapper::map(const Type& type) const {
    Type resolved = type;

    // Resolve typedef aliases.
    for (int i = 0; i < 8; ++i) {
        if (resolved.kind != TypeKind::Interface) break;
        const Typedef* td = m_db.findTypedef(resolved.name);
        if (!td) break;
        Type next = td->type;
        next.nullable = next.nullable || resolved.nullable;
        resolved = next;
    }

    TypeInfo info = mapResolved(resolved);

    // Nullable types are represented with std::optional and get their own
    // conversion helpers.  `any` and `object` are already nullable in JS.
    const bool supportsOptional = info.kind != TypeInfo::Kind::Void &&
                                   info.kind != TypeInfo::Kind::Value &&
                                   info.kind != TypeInfo::Kind::Object;
    if (info.nullable && supportsOptional) {
        info.cppType = "std::optional<" + info.cppType + ">";
        info.conversionSuffix = "Opt" + info.conversionSuffix;
    }

    return info;
}

TypeInfo TypeMapper::mapResolved(Type type) const {
    TypeInfo info;
    info.nullable = type.nullable;

    switch (type.kind) {
        case TypeKind::Void:
            info.kind = TypeInfo::Kind::Void;
            info.cppType = "void";
            info.conversionSuffix = "Void";
            return info;

        case TypeKind::Boolean:
            info.kind = TypeInfo::Kind::Bool;
            info.cppType = "bool";
            info.conversionSuffix = "Bool";
            return info;

        case TypeKind::Integer:
        case TypeKind::UnsignedInteger: {
            const bool isUnsigned = type.kind == TypeKind::UnsignedInteger;
            const std::string& name = type.name;
            if (name == "byte") {
                info.kind = TypeInfo::Kind::Int8;
                info.cppType = "int8_t";
            } else if (name == "short") {
                info.kind = TypeInfo::Kind::Int16;
                info.cppType = "int16_t";
            } else if (name == "long") {
                info.kind = TypeInfo::Kind::Int32;
                info.cppType = "int32_t";
            } else if (name == "long long") {
                info.kind = TypeInfo::Kind::Int64;
                info.cppType = "int64_t";
            } else if (name == "octet") {
                info.kind = TypeInfo::Kind::UInt8;
                info.cppType = "uint8_t";
            } else if (name == "unsigned short") {
                info.kind = TypeInfo::Kind::UInt16;
                info.cppType = "uint16_t";
            } else if (name == "unsigned long") {
                info.kind = TypeInfo::Kind::UInt32;
                info.cppType = "uint32_t";
            } else if (name == "unsigned long long") {
                info.kind = TypeInfo::Kind::UInt64;
                info.cppType = "uint64_t";
            } else {
                info.kind = TypeInfo::Kind::Int32;
                info.cppType = "int32_t";
            }
            if (!isUnsigned && info.kind == TypeInfo::Kind::UInt8) {
                info.kind = TypeInfo::Kind::Int8;
                info.cppType = "int8_t";
            }
            info.conversionSuffix =
                capitalize(info.kind == TypeInfo::Kind::Int8     ? "int8"
                           : info.kind == TypeInfo::Kind::Int16  ? "int16"
                           : info.kind == TypeInfo::Kind::Int32  ? "int32"
                           : info.kind == TypeInfo::Kind::Int64  ? "int64"
                           : info.kind == TypeInfo::Kind::UInt8  ? "uint8"
                           : info.kind == TypeInfo::Kind::UInt16 ? "uint16"
                           : info.kind == TypeInfo::Kind::UInt32 ? "uint32"
                                                                 : "uint64");
            return info;
        }

        case TypeKind::BigInt:
            info.kind = TypeInfo::Kind::BigInt;
            info.cppType = "int64_t";
            info.conversionSuffix = "BigInt";
            return info;

        case TypeKind::Float:
        case TypeKind::Double:
        case TypeKind::UnrestrictedFloat:
        case TypeKind::UnrestrictedDouble:
            info.kind = TypeInfo::Kind::Double;
            info.cppType = "double";
            info.conversionSuffix = "Double";
            return info;

        case TypeKind::String:
            info.kind = TypeInfo::Kind::String;
            info.cppType = "std::string";
            info.conversionSuffix = "String";
            return info;

        case TypeKind::Object:
            info.kind = TypeInfo::Kind::Object;
            info.cppType = "v8::Local<v8::Object>";
            info.conversionSuffix = "Object";
            return info;

        case TypeKind::Symbol:
        case TypeKind::Any:
        case TypeKind::Union:
        case TypeKind::Dictionary:
        case TypeKind::Callback:
        case TypeKind::Unknown:
            info.kind = TypeInfo::Kind::Value;
            info.cppType = "v8::Local<v8::Value>";
            info.conversionSuffix = "Value";
            return info;

        case TypeKind::Enum:
            info.kind = TypeInfo::Kind::String;
            info.cppType = "std::string";
            info.conversionSuffix = "String";
            return info;

        case TypeKind::Promise:
            info.kind = TypeInfo::Kind::Value;
            info.cppType = "v8::Local<v8::Value>";
            info.conversionSuffix = "Value";
            return info;

        case TypeKind::Sequence:
        case TypeKind::FrozenArray:
        case TypeKind::ObservableArray:
            info.kind = TypeInfo::Kind::Sequence;
            info.isSequence = true;
            if (!type.params.empty()) {
                const TypeInfo mapped = map(type.params[0]);
                info.element.kind = mapped.kind;
                info.element.nullable = mapped.nullable;
                info.element.cppType = mapped.cppType;
                info.element.conversionSuffix = mapped.conversionSuffix;
                info.element.interfaceName = mapped.interfaceName;
            } else {
                info.element.kind = TypeInfo::Kind::Value;
                info.element.cppType = "v8::Local<v8::Value>";
                info.element.conversionSuffix = "Value";
            }
            info.cppType = "std::vector<" + info.element.cppType + ">";
            info.conversionSuffix = "Seq" + info.element.conversionSuffix;
            return info;

        case TypeKind::Interface: {
            if (m_db.findInterface(type.name)) {
                info.kind = TypeInfo::Kind::Interface;
                info.interfaceName = type.name;
                info.cppType = "native::" + type.name + "*";
                info.conversionSuffix = type.name;
            } else if (m_db.findEnum(type.name)) {
                info.kind = TypeInfo::Kind::String;
                info.cppType = "std::string";
                info.conversionSuffix = "String";
            } else {
                info.kind = TypeInfo::Kind::Value;
                info.cppType = "v8::Local<v8::Value>";
                info.conversionSuffix = "Value";
            }
            return info;
        }
    }

    info.kind = TypeInfo::Kind::Value;
    info.cppType = "v8::Local<v8::Value>";
    info.conversionSuffix = "Value";
    return info;
}

}  // namespace webidlgen
