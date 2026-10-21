// Emits V8 binding code from a parsed WebIDL database.
#pragma once

#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "IdlAst.h"
#include "TypeMapper.h"

namespace webidlgen {

struct GeneratorOptions {
    // Name of the generated header, used for the #include in the source file.
    std::string headerFileName = "WebIDLBindings.h";
    std::vector<std::string> sources;
};

class CodeGenerator {
public:
    CodeGenerator(const Database& database, GeneratorOptions options);

    // Writes the header and the source file.  Returns false on I/O errors.
    bool generate(const std::string& headerPath, const std::string& sourcePath);

    const std::vector<std::string>& diagnostics() const { return m_diagnostics; }

private:
    struct Conversion {
        TypeInfo info;
    };

    // An attribute together with the interface that declares it.
    struct AttributeOwner {
        const Interface* owner = nullptr;
        const Member* member = nullptr;
    };

    // --- helpers ---
    void collectConversions();
    void requireConversion(const TypeInfo& info);
    void requireConversionRecursive(const TypeInfo& info);
    void requireMemberConversions(const TypeInfo& info);
    TypeInfo baseTypeInfo(const TypeInfo& info) const;

    std::string sanitize(const std::string& name) const;
    std::string capitalize(const std::string& name) const;
    std::string constantValueExpression(const Member& member) const;
    std::string defaultValueExpression(const Argument& argument) const;
    std::string zeroValueExpression(const TypeInfo& info) const;

    // True when the type is represented with std::optional.
    bool usesOptional(const TypeInfo& info) const;
    // C++ type without the std::optional wrapper.
    std::string underlyingType(const TypeInfo& info) const;

    const Interface* parentOf(const Interface& interface) const;
    std::vector<const Interface*> descendantsOf(const std::string& name) const;
    std::vector<const Interface*> sortedInterfaces() const;
    std::vector<AttributeOwner> allAttributes(const Interface& interface) const;
    bool isRedeclaration(const Interface& derived, const Member& member) const;

    std::string nativeClassName(const std::string& interfaceName) const;
    std::string bindingClassName(const std::string& interfaceName) const;

    // --- emission ---
    void emitHeader(std::ostream& out);
    void emitSource(std::ostream& out);
    void emitNativeInterface(std::ostream& out, const Interface& interface);
    void emitBindingClass(std::ostream& out, const Interface& interface);
    void emitInterfaceImplementation(std::ostream& out, const Interface& interface);
    void emitConversionDeclarations(std::ostream& out);
    void emitConversionDefinitions(std::ostream& out);
    void emitAttributeAccessor(std::ostream& out, const Interface& interface,
                               const Member& member);
    void emitOperationCallback(std::ostream& out, const Interface& interface,
                               const Member& member);
    void emitConstructorCallback(std::ostream& out, const Interface& interface,
                                 const Member& member);

    const Database& m_db;
    GeneratorOptions m_options;
    TypeMapper m_mapper;
    std::map<std::string, Conversion> m_conversions;
    std::vector<std::string> m_diagnostics;
    std::vector<const Interface*> m_order;
};

}  // namespace webidlgen
