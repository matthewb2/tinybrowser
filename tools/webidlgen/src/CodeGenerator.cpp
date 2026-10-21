#include "CodeGenerator.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <utility>

namespace webidlgen {
namespace {

const char* kCppKeywords[] = {
    "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
    "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
    "class", "compl", "concept", "const", "consteval", "constexpr",
    "constinit", "const_cast", "continue", "co_await", "co_return", "co_yield",
    "decltype", "default", "delete", "do", "double", "dynamic_cast", "else",
    "enum", "explicit", "export", "extern", "false", "float", "for", "friend",
    "goto", "if", "inline", "int", "long", "mutable", "namespace", "new",
    "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq",
    "private", "protected", "public", "register", "reinterpret_cast",
    "requires", "return", "short", "signed", "sizeof", "static", "static_assert",
    "static_cast", "struct", "switch", "template", "this", "thread_local",
    "throw", "true", "try", "typedef", "typeid", "typename", "union",
    "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while",
    "xor", "xor_eq"};

bool isCppKeyword(const std::string& word) {
    for (const char* keyword : kCppKeywords) {
        if (word == keyword) return true;
    }
    return false;
}

std::string escapeCppString(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 2);
    for (char c : text) {
        if (c == '"' || c == '\\') out.push_back('\\');
        if (c == '\n') {
            out += "\\n";
            continue;
        }
        if (c == '\t') {
            out += "\\t";
            continue;
        }
        if (c == '\r') {
            out += "\\r";
            continue;
        }
        out.push_back(c);
    }
    return out;
}

}  // namespace

CodeGenerator::CodeGenerator(const Database& database, GeneratorOptions options)
    : m_db(database),
      m_options(std::move(options)),
      m_mapper(database) {
    m_order = sortedInterfaces();
    collectConversions();
}

// ---------------------------------------------------------------------------
// Model helpers
// ---------------------------------------------------------------------------

std::string CodeGenerator::sanitize(const std::string& name) const {
    if (name.empty()) return "value";
    if (!isCppKeyword(name)) return name;
    return name + "_";
}

std::string CodeGenerator::capitalize(const std::string& name) const {
    if (name.empty()) return name;
    std::string out = name;
    out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    return out;
}

std::string CodeGenerator::nativeClassName(const std::string& interfaceName) const {
    return interfaceName;
}

std::string CodeGenerator::bindingClassName(const std::string& interfaceName) const {
    return interfaceName + "Binding";
}

const Interface* CodeGenerator::parentOf(const Interface& interface) const {
    if (interface.parent.empty()) return nullptr;
    return m_db.findInterface(interface.parent);
}

std::vector<const Interface*> CodeGenerator::descendantsOf(
    const std::string& name) const {
    std::vector<const Interface*> result;
    std::function<void(const std::string&)> collect = [&](const std::string& parent) {
        std::vector<const Interface*> direct;
        for (const Interface& candidate : m_db.interfaces) {
            if (candidate.parent == parent) direct.push_back(&candidate);
        }
        for (const Interface* child : direct) {
            collect(child->name);
            result.push_back(child);
        }
    };
    collect(name);
    std::stable_sort(result.begin(), result.end(),
                     [this](const Interface* a, const Interface* b) {
                         return m_db.inheritanceDepth(a->name) >
                                m_db.inheritanceDepth(b->name);
                     });
    return result;
}

std::vector<const Interface*> CodeGenerator::sortedInterfaces() const {
    std::vector<const Interface*> result;
    for (const Interface& iface : m_db.interfaces) {
        result.push_back(&iface);
    }
    std::stable_sort(result.begin(), result.end(),
                     [this](const Interface* a, const Interface* b) {
                         return m_db.inheritanceDepth(a->name) <
                                m_db.inheritanceDepth(b->name);
                     });
    return result;
}

std::vector<CodeGenerator::AttributeOwner> CodeGenerator::allAttributes(
    const Interface& interface) const {
    std::vector<AttributeOwner> result;

    // Walk the inheritance chain from the root down so that a derived
    // attribute shadows an inherited one with the same name.
    std::vector<const Interface*> chain;
    std::string parentName = interface.parent;
    for (int depth = 0; depth < 64 && !parentName.empty(); ++depth) {
        const Interface* parent = m_db.findInterface(parentName);
        if (!parent) break;
        chain.push_back(parent);
        parentName = parent->parent;
    }
    chain.push_back(&interface);

    for (const Interface* owner : chain) {
        for (const Member& member : owner->members) {
            if (member.kind != MemberKind::Attribute) continue;
            bool replaced = false;
            for (AttributeOwner& existing : result) {
                if (existing.member->name == member.name) {
                    existing.owner = owner;
                    existing.member = &member;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                AttributeOwner entry;
                entry.owner = owner;
                entry.member = &member;
                result.push_back(entry);
            }
        }
    }
    return result;
}

bool CodeGenerator::isRedeclaration(const Interface& derived,
                                    const Member& member) const {
    std::string parentName = derived.parent;
    for (int depth = 0; depth < 64 && !parentName.empty(); ++depth) {
        const Interface* parent = m_db.findInterface(parentName);
        if (!parent) break;
        for (const Member& other : parent->members) {
            if (other.name != member.name) continue;
            if (other.kind == MemberKind::Constant) continue;
            return true;
        }
        parentName = parent->parent;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Conversions
// ---------------------------------------------------------------------------

void CodeGenerator::requireConversion(const TypeInfo& info) {
    auto it = m_conversions.find(info.conversionSuffix);
    if (it == m_conversions.end()) {
        Conversion conversion;
        conversion.info = info;
        m_conversions.emplace(info.conversionSuffix, conversion);
        return;
    }
    if (it->second.info.cppType != info.cppType) {
        m_diagnostics.push_back("conflicting C++ types for conversion '" +
                                info.conversionSuffix + "': " +
                                it->second.info.cppType + " vs " + info.cppType);
    }
}

void CodeGenerator::requireConversionRecursive(const TypeInfo& info) {
    requireConversion(info);
    if (info.isSequence) {
        TypeInfo element;
        element.kind = info.element.kind;
        element.nullable = info.element.nullable;
        element.cppType = info.element.cppType;
        element.conversionSuffix = info.element.conversionSuffix;
        element.interfaceName = info.element.interfaceName;
        requireConversionRecursive(element);
    }
}

void CodeGenerator::collectConversions() {
    for (const Interface* iface : m_order) {
        for (const Member& member : iface->members) {
            switch (member.kind) {
                case MemberKind::Attribute:
                    requireMemberConversions(m_mapper.map(member.type));
                    break;
                case MemberKind::Operation:
                    requireMemberConversions(m_mapper.map(member.type));
                    for (const Argument& argument : member.arguments) {
                        requireMemberConversions(m_mapper.map(argument.type));
                    }
                    break;
                case MemberKind::Constructor:
                    for (const Argument& argument : member.arguments) {
                        requireMemberConversions(m_mapper.map(argument.type));
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

void CodeGenerator::requireMemberConversions(const TypeInfo& info) {
    requireConversionRecursive(info);
    // Nullable types delegate to the conversion of the underlying type, so
    // that conversion has to exist as well.
    if (usesOptional(info)) requireConversionRecursive(baseTypeInfo(info));
}

TypeInfo CodeGenerator::baseTypeInfo(const TypeInfo& info) const {
    TypeInfo base;
    base.kind = info.kind;
    base.nullable = false;
    base.isSequence = info.isSequence;
    base.cppType = underlyingType(info);
    base.conversionSuffix = info.conversionSuffix.rfind("Opt", 0) == 0
                                ? info.conversionSuffix.substr(3)
                                : info.conversionSuffix;
    base.interfaceName = info.interfaceName;
    base.element = info.element;
    return base;
}

bool CodeGenerator::usesOptional(const TypeInfo& info) const {
    if (!info.nullable) return false;
    switch (info.kind) {
        case TypeInfo::Kind::Void:
        case TypeInfo::Kind::Value:
        case TypeInfo::Kind::Object:
            return false;
        default:
            return true;
    }
}

std::string CodeGenerator::underlyingType(const TypeInfo& info) const {
    switch (info.kind) {
        case TypeInfo::Kind::Bool:
            return "bool";
        case TypeInfo::Kind::Int8:
        case TypeInfo::Kind::Int16:
        case TypeInfo::Kind::Int32:
        case TypeInfo::Kind::Int64:
        case TypeInfo::Kind::UInt8:
        case TypeInfo::Kind::UInt16:
        case TypeInfo::Kind::UInt32:
        case TypeInfo::Kind::UInt64:
        case TypeInfo::Kind::BigInt:
        case TypeInfo::Kind::Double:
            return info.cppType;
        case TypeInfo::Kind::String:
            return "std::string";
        case TypeInfo::Kind::Object:
            return "v8::Local<v8::Object>";
        case TypeInfo::Kind::Value:
            return "v8::Local<v8::Value>";
        case TypeInfo::Kind::Interface:
            return "native::" + info.interfaceName + "*";
        case TypeInfo::Kind::Sequence:
            return "std::vector<" + info.element.cppType + ">";
        case TypeInfo::Kind::Void:
            break;
    }
    return "void";
}

// ---------------------------------------------------------------------------
// Header
// ---------------------------------------------------------------------------

void CodeGenerator::emitHeader(std::ostream& out) {
    out << "// Generated by webidlgen - DO NOT EDIT.\n";
    if (!m_options.sources.empty()) {
        out << "//\n// WebIDL sources:\n";
        for (const std::string& source : m_options.sources) {
            out << "//   " << source << "\n";
        }
    }
    out << "\n#pragma once\n\n";
    out << "#include <v8.h>\n\n";
    out << "#include <cstdint>\n#include <optional>\n#include <string>\n";
    out << "#include <vector>\n\n";
    out << "namespace TinyBrowser {\nnamespace webidl {\n\n";

    // ---- native interfaces -------------------------------------------------
    out << "// ---------------------------------------------------------------------\n";
    out << "// Native interfaces implemented by the host application.\n";
    out << "// ---------------------------------------------------------------------\n";
    out << "namespace native {\n\n";
    for (const Interface* iface : m_order) {
        out << "class " << nativeClassName(iface->name) << ";\n";
    }
    out << "\n";
    for (const Interface* iface : m_order) {
        emitNativeInterface(out, *iface);
    }
    out << "}  // namespace native\n\n";

    // ---- forward declarations of the binding classes ------------------------
    out << "// Forward declarations of the generated binding classes.\n";
    for (const Interface* iface : m_order) {
        out << "class " << bindingClassName(iface->name) << ";\n";
    }
    out << "\n";

    // ---- conversion helpers ------------------------------------------------
    out << "// ---------------------------------------------------------------------\n";
    out << "// Conversion helpers.\n";
    out << "// ---------------------------------------------------------------------\n";
    out << "namespace detail {\n\n";
    out << "v8::Local<v8::String> ToV8StringValue(v8::Isolate* isolate, "
           "const std::string& value);\n";
    out << "std::string FromV8StringValue(v8::Isolate* isolate, "
           "v8::Local<v8::Value> value);\n";
    out << "v8::Local<v8::Name> ToV8Name(v8::Isolate* isolate, const char* value);\n";
    emitConversionDeclarations(out);
    out << "\n}  // namespace detail\n\n";

    // ---- binding classes ----------------------------------------------------
    out << "// ---------------------------------------------------------------------\n";
    out << "// Generated binding classes.\n";
    out << "// ---------------------------------------------------------------------\n";
    for (const Interface* iface : m_order) {
        emitBindingClass(out, *iface);
    }

    out << "// Installs every generated interface object on the global template.\n";
    out << "void InstallAll(v8::Isolate* isolate, "
           "v8::Local<v8::ObjectTemplate> global);\n\n";
    out << "// Releases the cached templates and prototypes.  Has to be called "
           "before the\n";
    out << "// isolate is disposed because the caches own persistent handles.\n";
    out << "void Reset();\n\n";
    out << "}  // namespace webidl\n}  // namespace TinyBrowser\n";
}

void CodeGenerator::emitNativeInterface(std::ostream& out,
                                        const Interface& interface) {
    const Interface* parent = parentOf(interface);
    out << "class " << nativeClassName(interface.name);
    if (parent) out << " : public " << nativeClassName(parent->name);
    out << " {\npublic:\n";
    out << "    virtual ~" << nativeClassName(interface.name) << "() = default;\n\n";

    for (const Member& member : interface.members) {
        switch (member.kind) {
            case MemberKind::Constant:
                break;  // constants are compile time values

            case MemberKind::Attribute: {
                const TypeInfo info = m_mapper.map(member.type);
                const std::string override =
                    isRedeclaration(interface, member) ? " override" : "";
                out << "    virtual " << info.cppType << " " << sanitize(member.name)
                    << "() const" << override << " = 0;\n";
                if (!member.isReadonly) {
                    out << "    virtual void set" << capitalize(sanitize(member.name))
                        << "(" << info.cppType << " value)" << override << " = 0;\n";
                }
                break;
            }

            case MemberKind::Operation: {
                const TypeInfo info = m_mapper.map(member.type);
                std::string params;
                for (const Argument& argument : member.arguments) {
                    const TypeInfo argInfo = m_mapper.map(argument.type);
                    if (!params.empty()) params += ", ";
                    if (argument.variadic) {
                        params += "const std::vector<" + argInfo.cppType + ">& " +
                                  sanitize(argument.name);
                    } else {
                        params += argInfo.cppType + " " + sanitize(argument.name);
                    }
                }
                const std::string override =
                    isRedeclaration(interface, member) ? " override" : "";
                out << "    virtual " << info.cppType << " " << sanitize(member.name)
                    << "(" << params << ")" << override << " = 0;\n";
                break;
            }

            case MemberKind::Constructor: {
                std::string params;
                for (const Argument& argument : member.arguments) {
                    const TypeInfo argInfo = m_mapper.map(argument.type);
                    if (!params.empty()) params += ", ";
                    params += argInfo.cppType + " " + sanitize(argument.name);
                }
                out << "    virtual " << nativeClassName(interface.name) << "* create("
                    << params << ") = 0;\n";
                break;
            }

            default:
                break;
        }
    }

    out << "};\n\n";
}

void CodeGenerator::emitBindingClass(std::ostream& out,
                                     const Interface& interface) {
    const std::string className = bindingClassName(interface.name);
    out << "class " << className << " {\npublic:\n";
    out << "    static constexpr const char* kInterfaceName = \"" << interface.name
        << "\";\n";
    out << "    static constexpr const char* kParentInterfaceName = \""
        << interface.parent << "\";\n\n";

    out << "    static void Install(v8::Isolate* isolate, "
           "v8::Local<v8::ObjectTemplate> global);\n";
    out << "    static v8::Local<v8::FunctionTemplate> GetConstructor("
           "v8::Isolate* isolate);\n";
    out << "    static v8::Local<v8::ObjectTemplate> GetInstanceTemplate("
           "v8::Isolate* isolate);\n";
    out << "    static v8::Local<v8::ObjectTemplate> GetPrototypeTemplate("
           "v8::Isolate* isolate);\n";
    out << "    static v8::Local<v8::Object> GetPrototypeObject("
           "v8::Isolate* isolate);\n";
    out << "    static v8::Local<v8::Object> Wrap(v8::Isolate* isolate, native::"
        << nativeClassName(interface.name) << "* value);\n";
    out << "    static native::" << nativeClassName(interface.name)
        << "* Unwrap(v8::Local<v8::Object> object);\n\n";

    for (const Member& member : interface.members) {
        switch (member.kind) {
            case MemberKind::Attribute: {
                if (member.isStatic) break;  // static attributes are not supported
                out << "    static void Get_" << sanitize(member.name)
                    << "(v8::Local<v8::Name> property, "
                       "const v8::PropertyCallbackInfo<v8::Value>& info);\n";
                if (!member.isReadonly) {
                    out << "    static void Set_" << sanitize(member.name)
                        << "(v8::Local<v8::Name> property, v8::Local<v8::Value> value, "
                           "const v8::PropertyCallbackInfo<void>& info);\n";
                }
                break;
            }
            case MemberKind::Operation:
                out << "    static void Op_" << sanitize(member.name)
                    << "(const v8::FunctionCallbackInfo<v8::Value>& args);\n";
                break;
            case MemberKind::Constructor:
                out << "    static void Op_constructor("
                       "const v8::FunctionCallbackInfo<v8::Value>& args);\n";
                break;
            default:
                break;
        }
    }

    out << "};\n\n";
}

// ---------------------------------------------------------------------------
// Source
// ---------------------------------------------------------------------------

void CodeGenerator::emitSource(std::ostream& out) {
    out << "// Generated by webidlgen - DO NOT EDIT.\n\n";
    out << "#include \"" << m_options.headerFileName << "\"\n\n";
    out << "#include <v8.h>\n\n";
    out << "namespace TinyBrowser {\nnamespace webidl {\n\n";

    emitConversionDefinitions(out);

    for (const Interface* iface : m_order) {
        emitInterfaceImplementation(out, *iface);
    }

    out << "void InstallAll(v8::Isolate* isolate, "
           "v8::Local<v8::ObjectTemplate> global) {\n";
    out << "    v8::HandleScope scope(isolate);\n";
    for (const Interface* iface : m_order) {
        if (iface->isMixin) continue;
        out << "    " << bindingClassName(iface->name)
            << "::Install(isolate, global);\n";
    }
    out << "}\n\n";

    out << "void Reset() {\n";
    for (const Interface* iface : m_order) {
        out << "    g_" << iface->name << "Constructor.Reset();\n";
        out << "    g_" << iface->name << "Prototype.Reset();\n";
    }
    out << "}\n\n";

    out << "}  // namespace webidl\n}  // namespace TinyBrowser\n";
}

void CodeGenerator::emitConversionDeclarations(std::ostream& out) {
    for (const auto& entry : m_conversions) {
        const TypeInfo& info = entry.second.info;
        const std::string& suffix = entry.first;
        const bool optional = usesOptional(info);
        const std::string base = underlyingType(info);

        if (info.kind == TypeInfo::Kind::Void) continue;

        if (optional) {
            out << "v8::Local<v8::Value> ToV8" << suffix
                << "(v8::Isolate* isolate, const std::optional<" << base
                << ">& value);\n";
            out << "std::optional<" << base << "> FromV8" << suffix
                << "(v8::Isolate* isolate, v8::Local<v8::Value> value);\n";
            continue;
        }

        switch (info.kind) {
            case TypeInfo::Kind::String:
                out << "v8::Local<v8::String> ToV8" << suffix
                    << "(v8::Isolate* isolate, const std::string& value);\n";
                break;
            case TypeInfo::Kind::Object:
                out << "v8::Local<v8::Value> ToV8" << suffix
                    << "(v8::Isolate* isolate, v8::Local<v8::Object> value);\n";
                break;
            case TypeInfo::Kind::Interface:
                out << "v8::Local<v8::Object> ToV8" << suffix
                    << "(v8::Isolate* isolate, native::" << info.interfaceName
                    << "* value);\n";
                break;
            case TypeInfo::Kind::Sequence:
                out << "v8::Local<v8::Array> ToV8" << suffix
                    << "(v8::Isolate* isolate, const " << base << "& value);\n";
                break;
            case TypeInfo::Kind::Value:
                out << "v8::Local<v8::Value> ToV8" << suffix
                    << "(v8::Isolate* isolate, v8::Local<v8::Value> value);\n";
                break;
            default:
                out << "v8::Local<v8::Value> ToV8" << suffix
                    << "(v8::Isolate* isolate, " << base << " value);\n";
                break;
        }

        out << base << " FromV8" << suffix
            << "(v8::Isolate* isolate, v8::Local<v8::Value> value);\n";
    }
}

void CodeGenerator::emitConversionDefinitions(std::ostream& out) {
    out << "namespace detail {\n\n";

    out << "v8::Local<v8::Name> ToV8Name(v8::Isolate* isolate, const char* value) {\n";
    out << "    return v8::String::NewFromUtf8(isolate, value).ToLocalChecked();\n";
    out << "}\n\n";

    out << "v8::Local<v8::String> ToV8StringValue(v8::Isolate* isolate, "
           "const std::string& value) {\n";
    out << "    return v8::String::NewFromUtf8(isolate, value.c_str()).ToLocalChecked();\n";
    out << "}\n\n";

    out << "std::string FromV8StringValue(v8::Isolate* isolate, "
           "v8::Local<v8::Value> value) {\n";
    out << "    if (value.IsEmpty() || value->IsNullOrUndefined()) return std::string();\n";
    out << "    v8::Local<v8::Context> context = isolate->GetCurrentContext();\n";
    out << "    v8::Local<v8::String> text;\n";
    out << "    if (!value->ToString(context).ToLocal(&text)) return std::string();\n";
    out << "    v8::String::Utf8Value utf8(isolate, text);\n";
    out << "    if (*utf8 == nullptr) return std::string();\n";
    out << "    return std::string(*utf8, static_cast<size_t>(utf8.length()));\n";
    out << "}\n\n";

    for (const auto& entry : m_conversions) {
        const TypeInfo& info = entry.second.info;
        const std::string& suffix = entry.first;
        const bool optional = usesOptional(info);
        const std::string base = underlyingType(info);

        if (info.kind == TypeInfo::Kind::Void) continue;

        if (optional) {
            // The optional variants delegate to the conversion of the
            // underlying (non nullable) type.
            const std::string baseSuffix = suffix.rfind("Opt", 0) == 0
                                              ? suffix.substr(3)
                                              : suffix;
            out << "v8::Local<v8::Value> ToV8" << suffix
                << "(v8::Isolate* isolate, const std::optional<" << base
                << ">& value) {\n";
            out << "    if (!value.has_value()) return v8::Null(isolate);\n";
            if (info.kind == TypeInfo::Kind::Interface) {
                out << "    if (*value == nullptr) return v8::Null(isolate);\n";
            }
            out << "    return v8::Local<v8::Value>(ToV8" << baseSuffix
                << "(isolate, *value));\n";
            out << "}\n\n";

            out << "std::optional<" << base << "> FromV8" << suffix
                << "(v8::Isolate* isolate, v8::Local<v8::Value> value) {\n";
            out << "    if (value.IsEmpty() || value->IsNullOrUndefined()) "
                   "return std::nullopt;\n";
            out << "    return static_cast<" << base << ">(FromV8" << baseSuffix
                << "(isolate, value));\n";
            out << "}\n\n";
            continue;
        }

        switch (info.kind) {
            case TypeInfo::Kind::Void:
                break;
            case TypeInfo::Kind::Bool:
                out << "v8::Local<v8::Value> ToV8" << suffix
                    << "(v8::Isolate* isolate, bool value) {\n";
                out << "    return v8::Boolean::New(isolate, value);\n}\n\n";
                out << "bool FromV8" << suffix
                    << "(v8::Isolate* isolate, v8::Local<v8::Value> value) {\n";
                out << "    if (value.IsEmpty()) return false;\n";
                out << "    return value->BooleanValue(isolate);\n}\n\n";
                break;

            case TypeInfo::Kind::Int8:
            case TypeInfo::Kind::Int16:
            case TypeInfo::Kind::Int32:
            case TypeInfo::Kind::Int64:
            case TypeInfo::Kind::UInt8:
            case TypeInfo::Kind::UInt16:
            case TypeInfo::Kind::UInt32:
            case TypeInfo::Kind::UInt64:
            case TypeInfo::Kind::BigInt:
            case TypeInfo::Kind::Double: {
                out << "v8::Local<v8::Value> ToV8" << suffix
                    << "(v8::Isolate* isolate, " << base << " value) {\n";
                if (info.kind == TypeInfo::Kind::Int8 ||
                    info.kind == TypeInfo::Kind::Int16 ||
                    info.kind == TypeInfo::Kind::Int32) {
                    out << "    return v8::Integer::New(isolate, "
                           "static_cast<int32_t>(value));\n";
                } else if (info.kind == TypeInfo::Kind::UInt8 ||
                           info.kind == TypeInfo::Kind::UInt16 ||
                           info.kind == TypeInfo::Kind::UInt32) {
                    out << "    return v8::Integer::NewFromUnsigned(isolate, "
                           "static_cast<uint32_t>(value));\n";
                } else if (info.kind == TypeInfo::Kind::Int64 ||
                           info.kind == TypeInfo::Kind::UInt64) {
                    out << "    return v8::BigInt::New(isolate, "
                           "static_cast<int64_t>(value));\n";
                } else if (info.kind == TypeInfo::Kind::BigInt) {
                    out << "    return v8::BigInt::New(isolate, "
                           "static_cast<int64_t>(value));\n";
                } else {
                    out << "    return v8::Number::New(isolate, "
                           "static_cast<double>(value));\n";
                }
                out << "}\n\n";

                out << base << " FromV8" << suffix
                    << "(v8::Isolate* isolate, v8::Local<v8::Value> value) {\n";
                out << "    if (value.IsEmpty() || !value->IsNumber()) return 0;\n";
                out << "    v8::Local<v8::Context> context = isolate->GetCurrentContext();\n";
                if (info.kind == TypeInfo::Kind::Int8 ||
                    info.kind == TypeInfo::Kind::Int16 ||
                    info.kind == TypeInfo::Kind::Int32) {
                    out << "    return static_cast<" << base
                        << ">(value->Int32Value(context).FromMaybe(0));\n";
                } else if (info.kind == TypeInfo::Kind::UInt8 ||
                           info.kind == TypeInfo::Kind::UInt16 ||
                           info.kind == TypeInfo::Kind::UInt32) {
                    out << "    return static_cast<" << base
                        << ">(value->Uint32Value(context).FromMaybe(0));\n";
                } else if (info.kind == TypeInfo::Kind::Int64 ||
                           info.kind == TypeInfo::Kind::UInt64 ||
                           info.kind == TypeInfo::Kind::BigInt) {
                    out << "    int64_t result = 0;\n";
                    out << "    if (!value->BigIntValue(context).To(&result)) return 0;\n";
                    out << "    return static_cast<" << base << ">(result);\n";
                } else {
                    out << "    return static_cast<" << base
                        << ">(value->NumberValue(context).FromMaybe(0.0));\n";
                }
                out << "}\n\n";
                break;
            }

            case TypeInfo::Kind::String:
                out << "v8::Local<v8::String> ToV8" << suffix
                    << "(v8::Isolate* isolate, const std::string& value) {\n";
                out << "    return ToV8StringValue(isolate, value);\n}\n\n";
                out << "std::string FromV8" << suffix
                    << "(v8::Isolate* isolate, v8::Local<v8::Value> value) {\n";
                out << "    return FromV8StringValue(isolate, value);\n}\n\n";
                break;

            case TypeInfo::Kind::Object:
                out << "v8::Local<v8::Value> ToV8" << suffix
                    << "(v8::Isolate* isolate, v8::Local<v8::Object> value) {\n";
                out << "    if (value.IsEmpty()) return v8::Null(isolate);\n";
                out << "    return v8::Local<v8::Value>(value);\n}\n\n";
                out << "v8::Local<v8::Object> FromV8" << suffix
                    << "(v8::Isolate* isolate, v8::Local<v8::Value> value) {\n";
                out << "    (void)isolate;\n";
                out << "    if (value.IsEmpty() || !value->IsObject()) "
                       "return v8::Local<v8::Object>();\n";
                out << "    return value.As<v8::Object>();\n}\n\n";
                break;

            case TypeInfo::Kind::Value:
                out << "v8::Local<v8::Value> ToV8" << suffix
                    << "(v8::Isolate* isolate, v8::Local<v8::Value> value) {\n";
                out << "    if (value.IsEmpty()) return v8::Undefined(isolate);\n";
                out << "    return value;\n}\n\n";
                out << "v8::Local<v8::Value> FromV8" << suffix
                    << "(v8::Isolate* isolate, v8::Local<v8::Value> value) {\n";
                out << "    if (value.IsEmpty()) return v8::Undefined(isolate);\n";
                out << "    return value;\n}\n\n";
                break;

            case TypeInfo::Kind::Interface:
                out << "v8::Local<v8::Object> ToV8" << suffix
                    << "(v8::Isolate* isolate, native::" << info.interfaceName
                    << "* value) {\n";
                out << "    if (value == nullptr) return v8::Local<v8::Object>();\n";
                out << "    return " << bindingClassName(info.interfaceName)
                    << "::Wrap(isolate, value);\n}\n\n";
                out << "native::" << info.interfaceName << "* FromV8" << suffix
                    << "(v8::Isolate* isolate, v8::Local<v8::Value> value) {\n";
                out << "    (void)isolate;\n";
                out << "    if (value.IsEmpty() || !value->IsObject()) return nullptr;\n";
                out << "    return " << bindingClassName(info.interfaceName)
                    << "::Unwrap(value.As<v8::Object>());\n}\n\n";
                break;

            case TypeInfo::Kind::Sequence: {
                const std::string& elementSuffix = info.element.conversionSuffix;
                out << "v8::Local<v8::Array> ToV8" << suffix
                    << "(v8::Isolate* isolate, const " << base << "& value) {\n";
                out << "    v8::Local<v8::Array> array = v8::Array::New(isolate, "
                       "static_cast<int>(value.size()));\n";
                out << "    v8::Local<v8::Context> context = isolate->GetCurrentContext();\n";
                out << "    for (size_t i = 0; i < value.size(); ++i) {\n";
                out << "        (void)array->Set(context, static_cast<uint32_t>(i), "
                       "v8::Local<v8::Value>(ToV8"
                    << elementSuffix << "(isolate, value[i])));\n";
                out << "    }\n";
                out << "    return array;\n}\n\n";
                out << base << " FromV8" << suffix
                    << "(v8::Isolate* isolate, v8::Local<v8::Value> value) {\n";
                out << "    " << base << " result;\n";
                out << "    if (value.IsEmpty() || !value->IsArray()) return result;\n";
                out << "    v8::Local<v8::Array> array = value.As<v8::Array>();\n";
                out << "    uint32_t length = array->Length();\n";
                out << "    result.reserve(length);\n";
                out << "    for (uint32_t i = 0; i < length; ++i) {\n";
                out << "        v8::Local<v8::Value> element;\n";
                out << "        if (!array->Get(isolate->GetCurrentContext(), i)."
                       "ToLocal(&element)) continue;\n";
                out << "        result.push_back(FromV8" << elementSuffix
                    << "(isolate, element));\n";
                out << "    }\n";
                out << "    return result;\n}\n\n";
                break;
            }
        }
    }

    out << "}  // namespace detail\n\n";
}

void CodeGenerator::emitInterfaceImplementation(std::ostream& out,
                                                const Interface& interface) {
    const std::string className = bindingClassName(interface.name);
    const std::string nativeName = nativeClassName(interface.name);

    out << "// ---------------------------------------------------------------------\n";
    out << "// " << interface.name << "\n";
    out << "// ---------------------------------------------------------------------\n";
    out << "namespace {\n";
    out << "v8::Global<v8::FunctionTemplate> g_" << interface.name
        << "Constructor;\n";
    out << "v8::Global<v8::Object> g_" << interface.name << "Prototype;\n";
    out << "}  // namespace\n\n";

    // ---- constructor -------------------------------------------------------
    out << "v8::Local<v8::FunctionTemplate> " << className
        << "::GetConstructor(v8::Isolate* isolate) {\n";
    out << "    if (g_" << interface.name << "Constructor.IsEmpty()) {\n";
    out << "        v8::HandleScope scope(isolate);\n";
    out << "        v8::Local<v8::FunctionTemplate> ctor = "
           "v8::FunctionTemplate::New(isolate);\n";
    out << "        ctor->SetClassName(v8::String::NewFromUtf8(isolate, \""
        << interface.name << "\").ToLocalChecked());\n";
    out << "        v8::Local<v8::ObjectTemplate> instance = ctor->InstanceTemplate();\n";
    bool hasOperations = false;
    for (const Member& member : interface.members) {
        if (member.kind == MemberKind::Operation) hasOperations = true;
    }
    if (hasOperations) {
        out << "        v8::Local<v8::ObjectTemplate> prototype = "
               "ctor->PrototypeTemplate();\n";
    }
    out << "        instance->SetInternalFieldCount(1);\n\n";

    // Attributes (own and inherited) become native data properties on the
    // instance so that the accessor callbacks receive the instance itself.
    for (const AttributeOwner& owner : allAttributes(interface)) {
        if (owner.member->isStatic) {
            m_diagnostics.push_back("static attribute '" + owner.member->name +
                                    "' of " + interface.name +
                                    " is not supported");
            continue;
        }
        out << "        instance->SetNativeDataProperty(detail::ToV8Name(isolate, \""
            << owner.member->name << "\"), "
            << bindingClassName(owner.owner->name) << "::Get_"
            << sanitize(owner.member->name);
        if (!owner.member->isReadonly) {
            out << ", " << bindingClassName(owner.owner->name) << "::Set_"
                << sanitize(owner.member->name);
        }
        out << ");\n";
    }

    // Operations live on the prototype object.
    for (const Member& member : interface.members) {
        if (member.kind != MemberKind::Operation) continue;
        if (member.isStatic) {
            out << "        ctor->Set(detail::ToV8Name(isolate, \"" << member.name
                << "\"), v8::FunctionTemplate::New(isolate, Op_"
                << sanitize(member.name) << "));\n";
        } else {
            out << "        prototype->Set(detail::ToV8Name(isolate, \"" << member.name
                << "\"), v8::FunctionTemplate::New(isolate, Op_"
                << sanitize(member.name) << "));\n";
        }
    }

    // Constants.
    for (const Member& member : interface.members) {
        if (member.kind != MemberKind::Constant) continue;
        out << "        ctor->Set(detail::ToV8Name(isolate, \"" << member.name
            << "\"), " << constantValueExpression(member) << ");\n";
    }

    // Constructors.
    for (const Member& member : interface.members) {
        if (member.kind != MemberKind::Constructor) continue;
        out << "        ctor->SetCallAsConstructorCallback(Op_constructor);\n";
    }

    out << "\n";
    out << "        g_" << interface.name << "Constructor.Reset(isolate, ctor);\n";
    out << "    }\n";
    out << "    return v8::Local<v8::FunctionTemplate>::New(isolate, g_"
        << interface.name << "Constructor);\n";
    out << "}\n\n";

    out << "v8::Local<v8::ObjectTemplate> " << className
        << "::GetInstanceTemplate(v8::Isolate* isolate) {\n";
    out << "    return GetConstructor(isolate)->InstanceTemplate();\n";
    out << "}\n\n";

    out << "v8::Local<v8::ObjectTemplate> " << className
        << "::GetPrototypeTemplate(v8::Isolate* isolate) {\n";
    out << "    return GetConstructor(isolate)->PrototypeTemplate();\n";
    out << "}\n\n";

    // ---- prototype object ---------------------------------------------------
    out << "v8::Local<v8::Object> " << className
        << "::GetPrototypeObject(v8::Isolate* isolate) {\n";
    out << "    if (g_" << interface.name << "Prototype.IsEmpty()) {\n";
    out << "        v8::HandleScope scope(isolate);\n";
    out << "        v8::Local<v8::Context> context = isolate->GetCurrentContext();\n";
    out << "        v8::Local<v8::Function> function = "
           "GetConstructor(isolate)->GetFunction(context).ToLocalChecked();\n";
    out << "        v8::Local<v8::Value> prototypeValue;\n";
    out << "        v8::Local<v8::String> prototypeKey = "
           "v8::String::NewFromUtf8(isolate, \"prototype\").ToLocalChecked();\n";
    out << "        if (!function->Get(context, prototypeKey).ToLocal(&prototypeValue)) "
           "return v8::Local<v8::Object>();\n";
    out << "        v8::Local<v8::Object> prototype = prototypeValue.As<v8::Object>();\n";
    const Interface* parent = parentOf(interface);
    if (parent) {
        out << "        v8::Local<v8::Object> parentPrototype = "
            << bindingClassName(parent->name) << "::GetPrototypeObject(isolate);\n";
        out << "        if (!parentPrototype.IsEmpty()) {\n";
        out << "            const v8::Maybe<bool> updated = "
               "prototype->SetPrototypeV2(context, parentPrototype);\n";
        out << "            (void)updated;\n";
        out << "        }\n";
    }
    out << "        g_" << interface.name << "Prototype.Reset(isolate, prototype);\n";
    out << "    }\n";
    out << "    return v8::Local<v8::Object>::New(isolate, g_" << interface.name
        << "Prototype);\n";
    out << "}\n\n";

    out << "void " << className << "::Install(v8::Isolate* isolate, "
           "v8::Local<v8::ObjectTemplate> global) {\n";
    out << "    global->Set(detail::ToV8Name(isolate, \"" << interface.name
        << "\"), GetConstructor(isolate));\n";
    out << "}\n\n";

    // ---- wrap / unwrap ------------------------------------------------------
    out << "v8::Local<v8::Object> " << className << "::Wrap(v8::Isolate* isolate, "
        << "native::" << nativeName << "* value) {\n";
    out << "    if (value == nullptr) return v8::Local<v8::Object>();\n";
    for (const Interface* derived : descendantsOf(interface.name)) {
        out << "    if (auto* typed = dynamic_cast<native::"
            << nativeClassName(derived->name) << "*>(value)) {\n";
        out << "        return " << bindingClassName(derived->name)
            << "::Wrap(isolate, typed);\n";
        out << "    }\n";
    }
    out << "    v8::Local<v8::Context> context = isolate->GetCurrentContext();\n";
    out << "    v8::Local<v8::ObjectTemplate> tmpl = GetInstanceTemplate(isolate);\n";
    out << "    v8::Local<v8::Object> object = "
           "tmpl->NewInstance(context).ToLocalChecked();\n";
    out << "    v8::Local<v8::Object> prototype = GetPrototypeObject(isolate);\n";
    out << "    if (!prototype.IsEmpty()) {\n";
    out << "        const v8::Maybe<bool> updated = "
           "object->SetPrototypeV2(context, prototype);\n";
    out << "        (void)updated;\n";
    out << "    }\n";
    out << "    object->SetInternalField(0, v8::External::New(isolate, "
           "static_cast<void*>(value)));\n";
    out << "    return object;\n";
    out << "}\n\n";

    out << "native::" << nativeName << "* " << className
        << "::Unwrap(v8::Local<v8::Object> object) {\n";
    out << "    if (object.IsEmpty() || object->InternalFieldCount() == 0) "
           "return nullptr;\n";
    out << "    v8::Local<v8::Data> field = object->GetInternalField(0);\n";
    out << "    if (field.IsEmpty() || !field->IsValue()) return nullptr;\n";
    out << "    v8::Local<v8::Value> value = field.As<v8::Value>();\n";
    out << "    if (!value->IsExternal()) return nullptr;\n";
    out << "    void* raw = value.As<v8::External>()->Value();\n";
    out << "    return dynamic_cast<native::" << nativeName
        << "*>(static_cast<native::" << nativeName << "*>(raw));\n";
    out << "}\n\n";

    // ---- attribute accessors -------------------------------------------------
    for (const Member& member : interface.members) {
        if (member.kind != MemberKind::Attribute) continue;
        if (member.isStatic) continue;
        emitAttributeAccessor(out, interface, member);
    }

    // ---- operations ----------------------------------------------------------
    for (const Member& member : interface.members) {
        if (member.kind != MemberKind::Operation) continue;
        emitOperationCallback(out, interface, member);
    }

    // ---- constructors --------------------------------------------------------
    for (const Member& member : interface.members) {
        if (member.kind != MemberKind::Constructor) continue;
        emitConstructorCallback(out, interface, member);
    }
}

void CodeGenerator::emitAttributeAccessor(std::ostream& out,
                                          const Interface& interface,
                                          const Member& member) {
    const std::string className = bindingClassName(interface.name);
    const TypeInfo info = m_mapper.map(member.type);
    const std::string selfType =
        "native::" + nativeClassName(interface.name) + "*";

    out << "void " << className << "::Get_" << sanitize(member.name)
        << "(v8::Local<v8::Name> property, "
           "const v8::PropertyCallbackInfo<v8::Value>& info) {\n";
    out << "    (void)property;\n";
    out << "    v8::Isolate* isolate = info.GetIsolate();\n";
    out << "    v8::HandleScope scope(isolate);\n";
    out << "    " << selfType << " self = Unwrap(info.HolderV2());\n";
    out << "    if (self == nullptr) {\n";
    out << "        info.GetReturnValue().SetUndefined();\n";
    out << "        return;\n";
    out << "    }\n";
    out << "    info.GetReturnValue().Set(v8::Local<v8::Value>(detail::ToV8"
        << info.conversionSuffix << "(isolate, self->" << sanitize(member.name)
        << "())));\n";
    out << "}\n\n";

    if (member.isReadonly) return;

    out << "void " << className << "::Set_" << sanitize(member.name)
        << "(v8::Local<v8::Name> property, v8::Local<v8::Value> value, "
           "const v8::PropertyCallbackInfo<void>& info) {\n";
    out << "    (void)property;\n";
    out << "    v8::Isolate* isolate = info.GetIsolate();\n";
    out << "    v8::HandleScope scope(isolate);\n";
    out << "    " << selfType << " self = Unwrap(info.HolderV2());\n";
    out << "    if (self == nullptr) return;\n";
    out << "    self->set" << capitalize(sanitize(member.name))
        << "(detail::FromV8" << info.conversionSuffix << "(isolate, value));\n";
    out << "}\n\n";
}

void CodeGenerator::emitOperationCallback(std::ostream& out,
                                          const Interface& interface,
                                          const Member& member) {
    const std::string className = bindingClassName(interface.name);
    const TypeInfo returnInfo = m_mapper.map(member.type);
    const std::string selfType =
        "native::" + nativeClassName(interface.name) + "*";

    // Number of arguments that must be present.
    size_t required = 0;
    while (required < member.arguments.size() &&
           !member.arguments[required].optional &&
           !member.arguments[required].variadic) {
        ++required;
    }

    out << "void " << className << "::Op_" << sanitize(member.name)
        << "(const v8::FunctionCallbackInfo<v8::Value>& args) {\n";
    out << "    v8::Isolate* isolate = args.GetIsolate();\n";
    out << "    v8::HandleScope scope(isolate);\n";
    out << "    " << selfType << " self = Unwrap(args.This());\n";
    out << "    if (self == nullptr) {\n";
    out << "        args.GetReturnValue().SetUndefined();\n";
    out << "        return;\n";
    out << "    }\n";
    if (required > 0) {
        out << "    if (args.Length() < " << required << ") {\n";
        out << "        isolate->ThrowException(v8::Exception::TypeError("
               "v8::String::NewFromUtf8(isolate, \""
            << interface.name << "." << member.name
            << "(): not enough arguments\").ToLocalChecked()));\n";
        out << "        return;\n";
        out << "    }\n";
    }

    // Decode the arguments.
    for (size_t i = 0; i < member.arguments.size(); ++i) {
        const Argument& argument = member.arguments[i];
        const TypeInfo argInfo = m_mapper.map(argument.type);
        const std::string variable = sanitize(argument.name);

        if (argument.variadic) {
            out << "    std::vector<" << argInfo.cppType << "> " << variable
                << ";\n";
            out << "    for (int i = " << i << "; i < args.Length(); ++i) {\n";
            out << "        " << variable
                << ".push_back(detail::FromV8" << argInfo.conversionSuffix
                << "(isolate, args[i]));\n";
            out << "    }\n";
            continue;
        }

        if (argument.optional && argument.hasDefault) {
            out << "    " << argInfo.cppType << " " << variable << " = "
                << defaultValueExpression(argument) << ";\n";
            out << "    if (args.Length() > " << i << " && !args[" << i
                << "]->IsUndefined()) {\n";
            out << "        " << variable << " = detail::FromV8"
                << argInfo.conversionSuffix << "(isolate, args[" << i << "]);\n";
            out << "    }\n";
            continue;
        }

        if (argument.optional) {
            out << "    " << argInfo.cppType << " " << variable << " = "
                << zeroValueExpression(argInfo) << ";\n";
            out << "    if (args.Length() > " << i << ") {\n";
            out << "        " << variable << " = detail::FromV8"
                << argInfo.conversionSuffix << "(isolate, args[" << i << "]);\n";
            out << "    }\n";
            continue;
        }

        out << "    " << argInfo.cppType << " " << variable << " = detail::FromV8"
            << argInfo.conversionSuffix << "(isolate, args[" << i << "]);\n";
    }

    // Build the call.
    std::string call = "self->" + sanitize(member.name) + "(";
    for (size_t i = 0; i < member.arguments.size(); ++i) {
        if (i > 0) call += ", ";
        call += sanitize(member.arguments[i].name);
    }
    call += ")";

    if (returnInfo.kind == TypeInfo::Kind::Void) {
        out << "    " << call << ";\n";
        out << "    args.GetReturnValue().SetUndefined();\n";
    } else {
        out << "    auto result = " << call << ";\n";
        out << "    args.GetReturnValue().Set(v8::Local<v8::Value>(detail::ToV8"
            << returnInfo.conversionSuffix << "(isolate, result)));\n";
    }
    out << "}\n\n";
}

void CodeGenerator::emitConstructorCallback(std::ostream& out,
                                            const Interface& interface,
                                            const Member& member) {
    const std::string className = bindingClassName(interface.name);
    (void)member;

    out << "void " << className
        << "::Op_constructor(const v8::FunctionCallbackInfo<v8::Value>& args) {\n";
    out << "    v8::Isolate* isolate = args.GetIsolate();\n";
    out << "    v8::HandleScope scope(isolate);\n";
    out << "    (void)args;\n";
    out << "    (void)member;\n";
    out << "    isolate->ThrowException(v8::Exception::TypeError("
           "v8::String::NewFromUtf8(isolate, \""
        << interface.name
        << " is not constructible\").ToLocalChecked()));\n";
    out << "}\n\n";
}

// ---------------------------------------------------------------------------
// Constant values
// ---------------------------------------------------------------------------

std::string CodeGenerator::zeroValueExpression(const TypeInfo& info) const {
    switch (info.kind) {
        case TypeInfo::Kind::Bool:
            return "false";
        case TypeInfo::Kind::String:
            return "std::string()";
        case TypeInfo::Kind::Interface:
            return "nullptr";
        case TypeInfo::Kind::Object:
        case TypeInfo::Kind::Value:
            return "v8::Local<v8::Value>()";
        case TypeInfo::Kind::Sequence:
            return "std::vector<" + info.element.cppType + ">()";
        default:
            return "0";
    }
}

std::string CodeGenerator::defaultValueExpression(const Argument& argument) const {
    const TypeInfo info = m_mapper.map(argument.type);
    const std::string& value = argument.defaultValue;

    if (value == "true") return "true";
    if (value == "false") return "false";
    if (value == "null") return zeroValueExpression(info);
    if (value.empty()) return zeroValueExpression(info);

    if (info.kind == TypeInfo::Kind::String) {
        return "\"" + escapeCppString(value) + "\"";
    }
    if (info.kind == TypeInfo::Kind::Interface) {
        return "nullptr";
    }
    if (info.kind == TypeInfo::Kind::Bool) {
        return value == "true" ? "true" : "false";
    }
    if (info.kind == TypeInfo::Kind::Double) {
        return value;
    }
    if (!value.empty() && (std::isdigit(static_cast<unsigned char>(value[0])) ||
                           value[0] == '-' || value[0] == '+')) {
        return value;
    }
    return zeroValueExpression(info);
}

std::string CodeGenerator::constantValueExpression(const Member& member) const {
    const TypeInfo info = m_mapper.map(member.type);
    const std::string& value = member.constantValue;

    if (info.kind == TypeInfo::Kind::String) {
        return "detail::ToV8String(isolate, \"" + escapeCppString(value) + "\")";
    }
    if (info.kind == TypeInfo::Kind::Bool) {
        return std::string("v8::Boolean::New(isolate, ") +
               (value == "true" ? "true" : "false") + ")";
    }
    if (info.kind == TypeInfo::Kind::Double) {
        return "v8::Number::New(isolate, " + value + ")";
    }
    if (info.kind == TypeInfo::Kind::UInt8 || info.kind == TypeInfo::Kind::UInt16 ||
        info.kind == TypeInfo::Kind::UInt32) {
        return "v8::Integer::NewFromUnsigned(isolate, " + value + ")";
    }
    return "v8::Integer::New(isolate, " + value + ")";
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

bool CodeGenerator::generate(const std::string& headerPath,
                             const std::string& sourcePath) {
    std::ofstream header(headerPath, std::ios::binary);
    if (!header) {
        m_diagnostics.push_back("cannot open '" + headerPath + "' for writing");
        return false;
    }
    emitHeader(header);
    header.close();

    std::ofstream source(sourcePath, std::ios::binary);
    if (!source) {
        m_diagnostics.push_back("cannot open '" + sourcePath + "' for writing");
        return false;
    }
    emitSource(source);
    source.close();

    return true;
}

}  // namespace webidlgen
