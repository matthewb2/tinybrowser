#include "IdlParser.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace webidlgen {

// ---------------------------------------------------------------------------
// Database helpers
// ---------------------------------------------------------------------------

const Interface* Database::findInterface(const std::string& name) const {
    for (const Interface& iface : interfaces) {
        if (iface.name == name) return &iface;
    }
    return nullptr;
}

const Enum* Database::findEnum(const std::string& name) const {
    for (const Enum& e : enums) {
        if (e.name == name) return &e;
    }
    return nullptr;
}

const Dictionary* Database::findDictionary(const std::string& name) const {
    for (const Dictionary& d : dictionaries) {
        if (d.name == name) return &d;
    }
    return nullptr;
}

const Typedef* Database::findTypedef(const std::string& name) const {
    for (const Typedef& t : typedefs) {
        if (t.name == name) return &t;
    }
    return nullptr;
}

const Callback* Database::findCallback(const std::string& name) const {
    for (const Callback& c : callbacks) {
        if (c.name == name) return &c;
    }
    return nullptr;
}

int Database::inheritanceDepth(const std::string& name) const {
    const Interface* iface = findInterface(name);
    if (!iface) return 0;
    int depth = 0;
    std::string parent = iface->parent;
    while (!parent.empty() && depth < 64) {
        const Interface* p = findInterface(parent);
        if (!p) break;
        ++depth;
        parent = p->parent;
    }
    return depth;
}

std::vector<const Interface*> Database::subclassesOf(
    const std::string& name) const {
    std::vector<const Interface*> result;
    for (const Interface& iface : interfaces) {
        if (iface.parent == name) result.push_back(&iface);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

Parser::Parser(std::string source, std::string filename)
    : m_filename(std::move(filename)) {
    Lexer lexer(std::move(source), m_filename);
    lexer.tokenize();
    m_tokens = lexer.tokens();
    for (const std::string& e : lexer.errors()) m_errors.push_back(e);
}

const Token& Parser::lookahead(size_t offset) const {
    size_t index = m_index + offset;
    if (index >= m_tokens.size()) return m_tokens.back();
    return m_tokens[index];
}

Token Parser::advance() {
    Token token = current();
    if (!atEnd()) ++m_index;
    return token;
}

bool Parser::matchSymbol(const char* s) {
    if (!checkSymbol(s)) return false;
    advance();
    return true;
}

bool Parser::matchKeyword(const char* s) {
    if (!checkKeyword(s)) return false;
    advance();
    return true;
}

bool Parser::expectSymbol(const char* s) {
    if (matchSymbol(s)) return true;
    error(std::string("expected '") + s + "' but found '" + current().text + "'");
    return false;
}

bool Parser::expectKeyword(const char* s) {
    if (matchKeyword(s)) return true;
    error(std::string("expected '") + s + "' but found '" + current().text + "'");
    return false;
}

void Parser::error(const std::string& message) { errorAt(current(), message); }

void Parser::errorAt(const Token& token, const std::string& message) {
    m_errors.push_back(m_filename + ":" + std::to_string(token.line) + ":" +
                       std::to_string(token.column) + ": " + message);
}

bool Parser::parse(Database& db) {
    parseDefinitions(db);

    // Resolve `partial interface` members into the base declaration.
    // parseInterface already merges when the interface is known, so nothing
    // else is required here.
    for (const std::string& e : m_errors) db.diagnostics.push_back(e);
    return m_errors.empty();
}

void Parser::parseDefinitions(Database& db) {
    while (!atEnd()) {
        if (matchSymbol(";")) continue;

        if (checkSymbol("[")) {
            // Extended attributes in front of a definition.
            std::vector<ExtendedAttribute> attrs = parseExtendedAttributes();
            if (checkKeyword("interface")) {
                parseInterface(db, false, false, attrs);
            } else if (checkKeyword("dictionary")) {
                parseDictionary(db, false);
            } else if (checkKeyword("enum")) {
                parseEnum(db, false);
            } else if (checkKeyword("callback")) {
                parseCallback(db);
            } else if (checkKeyword("typedef")) {
                parseTypedef(db);
            } else {
                error("extended attributes must be followed by a definition");
                // Skip to the next ';' to recover.
                while (!atEnd() && !checkSymbol(";")) advance();
                matchSymbol(";");
            }
            continue;
        }

        if (checkKeyword("interface")) {
            parseInterface(db, false, false, {});
        } else if (checkKeyword("partial")) {
            advance();
            if (checkKeyword("interface")) {
                parseInterface(db, true, false, {});
            } else if (checkKeyword("dictionary")) {
                parseDictionary(db, true);
            } else if (checkKeyword("enum")) {
                parseEnum(db, true);
            } else {
                error("'partial' must be followed by interface, dictionary or enum");
                while (!atEnd() && !checkSymbol(";")) advance();
                matchSymbol(";");
            }
        } else if (checkKeyword("dictionary")) {
            parseDictionary(db, false);
        } else if (checkKeyword("enum")) {
            parseEnum(db, false);
        } else if (checkKeyword("callback")) {
            parseCallback(db);
        } else if (checkKeyword("typedef")) {
            parseTypedef(db);
        } else if (checkKeyword("namespace")) {
            parseNamespace(db);
        } else {
            error("unexpected token '" + current().text + "' at top level");
            while (!atEnd() && !checkSymbol(";")) advance();
            matchSymbol(";");
        }
    }
}

std::string Parser::parseIdentifier(const char* what) {
    if (!current().is(TokenType::Identifier)) {
        error(std::string("expected ") + what + " identifier");
        return std::string();
    }
    return advance().text;
}

std::vector<ExtendedAttribute> Parser::parseExtendedAttributes() {
    std::vector<ExtendedAttribute> result;
    if (!expectSymbol("[")) return result;

    while (!atEnd() && !checkSymbol("]")) {
        ExtendedAttribute attr;
        attr.name = parseIdentifier("extended attribute");
        if (matchSymbol("=")) {
            if (current().is(TokenType::Identifier)) {
                attr.hasValue = true;
                attr.value = advance().text;
                // Dotted names such as Exposed=Window.
                while (checkSymbol(".")) {
                    advance();
                    attr.value += "." + parseIdentifier("extended attribute");
                }
            } else if (current().is(TokenType::String)) {
                attr.hasValue = true;
                attr.value = advance().text;
            }
        } else if (checkSymbol("(")) {
            advance();
            while (!atEnd() && !checkSymbol(")")) {
                if (current().is(TokenType::Identifier) ||
                    current().is(TokenType::String)) {
                    attr.list.push_back(advance().text);
                } else {
                    advance();
                }
                if (!matchSymbol(",")) break;
            }
            expectSymbol(")");
        }
        result.push_back(std::move(attr));
        if (!matchSymbol(",")) break;
    }
    expectSymbol("]");
    return result;
}

void Parser::parseInterface(Database& db, bool partial, bool isMixin,
                            std::vector<ExtendedAttribute> extended) {
    advance();  // 'interface'

    bool mixin = isMixin;
    if (matchKeyword("mixin")) mixin = true;

    Interface iface;
    iface.name = parseIdentifier("interface");
    iface.partial = partial;
    iface.isMixin = mixin;
    iface.sourceFile = m_filename;

    if (matchSymbol(":")) {
        iface.parent = parseIdentifier("inherited interface");
    }

    iface.extended = std::move(extended);

    if (!expectSymbol("{")) {
        while (!atEnd() && !checkSymbol(";")) advance();
        matchSymbol(";");
        return;
    }

    parseMembers(iface);
    expectSymbol("}");
    matchSymbol(";");

    if (iface.name.empty()) return;

    // Merge partial declarations into an already known interface.
    Interface* existing = nullptr;
    for (Interface& candidate : db.interfaces) {
        if (candidate.name == iface.name) {
            existing = &candidate;
            break;
        }
    }

    if (existing != nullptr) {
        if (!iface.parent.empty() && existing->parent.empty()) {
            existing->parent = iface.parent;
        }
        for (Member& m : iface.members) existing->members.push_back(std::move(m));
        for (ExtendedAttribute& a : iface.extended) {
            existing->extended.push_back(std::move(a));
        }
        return;
    }

    db.interfaces.push_back(std::move(iface));
}

void Parser::parseDictionary(Database& db, bool partial) {
    advance();  // 'dictionary'

    Dictionary dict;
    dict.name = parseIdentifier("dictionary");
    dict.sourceFile = m_filename;
    dict.extended = parseExtendedAttributes();

    if (!expectSymbol("{")) {
        while (!atEnd() && !checkSymbol(";")) advance();
        matchSymbol(";");
        return;
    }

    parseMembers(dict);
    expectSymbol("}");
    matchSymbol(";");

    if (dict.name.empty()) return;

    for (Dictionary& candidate : db.dictionaries) {
        if (candidate.name == dict.name) {
            for (Member& m : dict.members) candidate.members.push_back(std::move(m));
            return;
        }
    }
    db.dictionaries.push_back(std::move(dict));
    (void)partial;
}

void Parser::parseEnum(Database& db, bool partial) {
    advance();  // 'enum'

    Enum e;
    e.name = parseIdentifier("enum");
    e.sourceFile = m_filename;

    if (!expectSymbol("{")) {
        while (!atEnd() && !checkSymbol(";")) advance();
        matchSymbol(";");
        return;
    }

    while (!atEnd() && !checkSymbol("}")) {
        if (current().is(TokenType::String)) {
            e.values.push_back(advance().text);
        } else {
            error("enum values must be string literals");
            advance();
        }
        if (!matchSymbol(",")) break;
    }
    expectSymbol("}");
    matchSymbol(";");

    if (e.name.empty()) return;

    for (Enum& candidate : db.enums) {
        if (candidate.name == e.name) {
            for (const std::string& v : e.values) candidate.values.push_back(v);
            return;
        }
    }
    db.enums.push_back(std::move(e));
    (void)partial;
}

void Parser::parseCallback(Database& db) {
    advance();  // 'callback'

    Callback cb;
    cb.sourceFile = m_filename;
    cb.name = parseIdentifier("callback");
    if (!expectSymbol("=")) {
        while (!atEnd() && !checkSymbol(";")) advance();
        matchSymbol(";");
        return;
    }
    cb.returnType = parseType();
    bool variadic = false;
    parseArguments(cb.arguments, variadic);
    matchSymbol(";");

    if (cb.name.empty()) return;
    db.callbacks.push_back(std::move(cb));
}

void Parser::parseTypedef(Database& db) {
    advance();  // 'typedef'

    Typedef td;
    td.sourceFile = m_filename;
    td.type = parseType();
    td.name = parseIdentifier("typedef");
    matchSymbol(";");

    if (td.name.empty()) return;
    db.typedefs.push_back(std::move(td));
}

void Parser::parseNamespace(Database& db) {
    advance();  // 'namespace'
    parseIdentifier("namespace");
    if (!expectSymbol("{")) {
        while (!atEnd() && !checkSymbol(";")) advance();
        matchSymbol(";");
        return;
    }
    int depth = 1;
    while (!atEnd() && depth > 0) {
        if (checkSymbol("{")) ++depth;
        if (checkSymbol("}")) --depth;
        advance();
    }
    (void)db;
}

// ---------------------------------------------------------------------------
// Members
// ---------------------------------------------------------------------------

void Parser::parseMembers(Interface& interface) {
    while (!atEnd() && !checkSymbol("}")) {
        size_t before = m_index;
        parseMember(interface);
        if (m_index == before) advance();  // guarantee progress
    }
}

void Parser::parseMembers(Dictionary& dictionary) {
    while (!atEnd() && !checkSymbol("}")) {
        size_t before = m_index;
        parseDictionaryMember(dictionary);
        if (m_index == before) advance();
    }
}

void Parser::parseDictionaryMember(Dictionary& dictionary) {
    if (checkSymbol("[")) parseExtendedAttributes();
    if (matchKeyword("required")) {
        // 'required' members are treated like normal members.
    }
    Member member;
    member.kind = MemberKind::Attribute;
    member.type = parseType();
    member.name = parseIdentifier("dictionary member");
    if (matchSymbol("=")) member.constantValue = parseConstantValue();
    matchSymbol(";");
    if (!member.name.empty()) dictionary.members.push_back(std::move(member));
}

bool Parser::parseMemberIsStatic() {
    if (!checkKeyword("static")) return false;
    advance();
    return true;
}

Member Parser::parseConstantMember() {
    Member member;
    member.kind = MemberKind::Constant;
    member.type = parseType();
    member.name = parseIdentifier("constant");
    if (expectSymbol("=")) {
        member.constantValue = parseConstantValue();
    }
    matchSymbol(";");
    return member;
}

Member Parser::parseAttributeMember(bool isStatic) {
    Member member;
    member.kind = MemberKind::Attribute;
    member.isStatic = isStatic;
    member.type = parseType();
    member.name = parseIdentifier("attribute");
    matchSymbol(";");
    return member;
}

Member Parser::parseOperationMember(bool isStatic, bool isAsync) {
    Member member;
    member.kind = MemberKind::Operation;
    member.isStatic = isStatic;
    member.isAsync = isAsync;
    member.type = parseType();
    member.name = parseIdentifier("operation");
    bool variadic = false;
    parseArguments(member.arguments, variadic);
    matchSymbol(";");
    return member;
}

void Parser::parseMember(Interface& interface) {
    std::vector<ExtendedAttribute> extended;
    if (checkSymbol("[")) extended = parseExtendedAttributes();

    // 'readonly' attribute
    if (checkKeyword("readonly")) {
        advance();
        if (!expectKeyword("attribute")) return;
        Member member = parseAttributeMember(false);
        member.isReadonly = true;
        member.extended = std::move(extended);
        interface.members.push_back(std::move(member));
        return;
    }

    // 'inherit' attribute
    if (checkKeyword("inherit")) {
        advance();
        if (!expectKeyword("attribute")) return;
        Member member = parseAttributeMember(false);
        member.isInherit = true;
        member.extended = std::move(extended);
        interface.members.push_back(std::move(member));
        return;
    }

    // 'const'
    if (checkKeyword("const")) {
        advance();
        Member member = parseConstantMember();
        member.extended = std::move(extended);
        interface.members.push_back(std::move(member));
        return;
    }

    // 'attribute'
    if (checkKeyword("attribute")) {
        advance();
        Member member = parseAttributeMember(false);
        member.extended = std::move(extended);
        interface.members.push_back(std::move(member));
        return;
    }

    // 'static'
    if (checkKeyword("static")) {
        advance();
        if (checkKeyword("attribute")) {
            advance();
            Member member = parseAttributeMember(true);
            member.extended = std::move(extended);
            interface.members.push_back(std::move(member));
            return;
        }
        if (checkKeyword("readonly")) {
            advance();
            if (!expectKeyword("attribute")) return;
            Member member = parseAttributeMember(true);
            member.isReadonly = true;
            member.extended = std::move(extended);
            interface.members.push_back(std::move(member));
            return;
        }
        Member member = parseOperationMember(true, false);
        member.extended = std::move(extended);
        interface.members.push_back(std::move(member));
        return;
    }

    // 'constructor'
    if (checkKeyword("constructor")) {
        advance();
        Member member;
        member.kind = MemberKind::Constructor;
        member.name = "constructor";
        bool variadic = false;
        parseArguments(member.arguments, variadic);
        matchSymbol(";");
        member.extended = std::move(extended);
        interface.members.push_back(std::move(member));
        return;
    }

    // 'stringifier'
    if (checkKeyword("stringifier")) {
        advance();
        Member member;
        member.kind = MemberKind::Stringifier;
        if (checkKeyword("attribute") || checkKeyword("readonly")) {
            bool readonly = matchKeyword("readonly");
            if (!expectKeyword("attribute")) return;
            member.kind = MemberKind::Attribute;
            member.isReadonly = readonly;
            member.type = parseType();
            member.name = parseIdentifier("attribute");
        } else if (current().is(TokenType::Identifier)) {
            member.kind = MemberKind::Operation;
            member.type = parseType();
            member.name = parseIdentifier("operation");
            bool variadic = false;
            parseArguments(member.arguments, variadic);
        }
        matchSymbol(";");
        member.extended = std::move(extended);
        interface.members.push_back(std::move(member));
        return;
    }

    // 'iterable' / 'maplike' / 'setlike'
    if (checkKeyword("iterable") || checkKeyword("maplike") ||
        checkKeyword("setlike")) {
        std::string keyword = advance().text;
        Member member;
        member.kind = keyword == "iterable" ? MemberKind::Iterable
                                            : (keyword == "maplike"
                                                   ? MemberKind::Maplike
                                                   : MemberKind::Setlike);
        member.name = keyword;
        if (matchSymbol("<")) {
            int depth = 1;
            while (!atEnd() && depth > 0) {
                if (checkSymbol("<")) ++depth;
                if (checkSymbol(">")) --depth;
                if (depth > 0) advance();
            }
            expectSymbol(">");
        }
        matchSymbol(";");
        member.extended = std::move(extended);
        interface.members.push_back(std::move(member));
        return;
    }

    // 'getter' / 'setter' / 'deleter'
    if (checkKeyword("getter") || checkKeyword("setter") ||
        checkKeyword("deleter")) {
        std::string keyword = advance().text;
        Member member;
        member.kind = keyword == "getter"   ? MemberKind::Getter
                       : keyword == "setter" ? MemberKind::Setter
                                             : MemberKind::Deleter;
        member.type = parseType();
        member.name = parseIdentifier("special member");
        matchSymbol(";");
        member.extended = std::move(extended);
        interface.members.push_back(std::move(member));
        return;
    }

    // 'async'
    if (checkKeyword("async")) {
        advance();
        Member member = parseOperationMember(false, true);
        member.extended = std::move(extended);
        interface.members.push_back(std::move(member));
        return;
    }

    // Plain operation: 'ReturnType name(args);'
    if (current().is(TokenType::Identifier)) {
        size_t before = m_index;
        Type returnType = parseType();
        if (current().is(TokenType::Identifier)) {
            Member member;
            member.kind = MemberKind::Operation;
            member.type = returnType;
            member.name = parseIdentifier("operation");
            bool variadic = false;
            parseArguments(member.arguments, variadic);
            matchSymbol(";");
            member.extended = std::move(extended);
            interface.members.push_back(std::move(member));
            return;
        }
        m_index = before;  // not an operation after all
    }

    error("unexpected token '" + current().text + "' in interface body");
    while (!atEnd() && !checkSymbol(";") && !checkSymbol("}")) advance();
    matchSymbol(";");
}

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

Type Parser::parseType() {
    Type type = parseTypeBase();
    if (matchSymbol("?")) type.nullable = true;
    if (checkSymbol("[") && lookahead().isSymbol("]")) {
        advance();
        advance();
        Type array;
        array.kind = TypeKind::Sequence;
        array.isArray = true;
        array.params.push_back(type);
        array.nullable = type.nullable;
        return array;
    }
    return type;
}

Type Parser::parseGenericType(const std::string& name) {
    Type type;
    if (name == "sequence") {
        type.kind = TypeKind::Sequence;
    } else if (name == "FrozenArray") {
        type.kind = TypeKind::FrozenArray;
    } else if (name == "ObservableArray") {
        type.kind = TypeKind::ObservableArray;
    } else if (name == "Promise") {
        type.kind = TypeKind::Promise;
    } else {
        type.kind = TypeKind::Unknown;
        type.name = name;
        return type;
    }

    if (!expectSymbol("<")) return type;
    Type element = parseType();
    type.params.push_back(element);
    expectSymbol(">");
    return type;
}

Type Parser::parseTypeBase() {
    if (!current().is(TokenType::Identifier)) {
        error("expected a type");
        Type unknown;
        unknown.kind = TypeKind::Unknown;
        return unknown;
    }

    std::string word = current().text;

    if (word == "void") { advance(); Type t; t.kind = TypeKind::Void; return t; }
    if (word == "boolean") { advance(); Type t; t.kind = TypeKind::Boolean; return t; }
    if (word == "byte") { advance(); Type t; t.kind = TypeKind::Integer; t.name = "byte"; return t; }
    if (word == "octet") { advance(); Type t; t.kind = TypeKind::UnsignedInteger; t.name = "octet"; return t; }
    if (word == "bigint") { advance(); Type t; t.kind = TypeKind::BigInt; return t; }
    if (word == "object") { advance(); Type t; t.kind = TypeKind::Object; return t; }
    if (word == "symbol") { advance(); Type t; t.kind = TypeKind::Symbol; return t; }
    if (word == "any") { advance(); Type t; t.kind = TypeKind::Any; return t; }
    if (word == "undefined") { advance(); Type t; t.kind = TypeKind::Unknown; t.name = "undefined"; return t; }

    if (word == "DOMString" || word == "ByteString" || word == "USVString") {
        advance();
        Type t;
        t.kind = TypeKind::String;
        t.name = word;
        return t;
    }

    if (word == "short") {
        advance();
        Type t;
        t.kind = TypeKind::Integer;
        t.name = "short";
        return t;
    }

    if (word == "long") {
        advance();
        Type t;
        t.kind = TypeKind::Integer;
        t.name = "long";
        if (matchKeyword("long")) t.name = "long long";
        return t;
    }

    if (word == "unsigned") {
        advance();
        Type t;
        t.kind = TypeKind::UnsignedInteger;
        if (matchKeyword("short")) {
            t.name = "unsigned short";
        } else if (matchKeyword("long")) {
            t.name = "unsigned long";
            if (matchKeyword("long")) t.name = "unsigned long long";
        } else {
            error("expected 'short' or 'long' after 'unsigned'");
        }
        return t;
    }

    if (word == "float") {
        advance();
        Type t;
        t.kind = TypeKind::Float;
        return t;
    }

    if (word == "double") {
        advance();
        Type t;
        t.kind = TypeKind::Double;
        return t;
    }

    if (word == "unrestricted") {
        advance();
        Type t;
        if (matchKeyword("float")) {
            t.kind = TypeKind::UnrestrictedFloat;
        } else if (matchKeyword("double")) {
            t.kind = TypeKind::UnrestrictedDouble;
        } else {
            error("expected 'float' or 'double' after 'unrestricted'");
        }
        return t;
    }

    if (word == "sequence" || word == "FrozenArray" ||
        word == "ObservableArray" || word == "Promise") {
        advance();
        return parseGenericType(word);
    }

    // Union type: (A or B or C)
    if (checkSymbol("(")) {
        advance();
        Type t;
        t.kind = TypeKind::Union;
        for (;;) {
            t.params.push_back(parseType());
            if (matchKeyword("or")) continue;
            break;
        }
        expectSymbol(")");
        return t;
    }

    // Reference to a named definition.
    advance();
    Type t;
    t.kind = TypeKind::Interface;
    t.name = word;
    return t;
}

// ---------------------------------------------------------------------------
// Arguments
// ---------------------------------------------------------------------------

bool Parser::parseArguments(std::vector<Argument>& arguments,
                            bool& variadicTail) {
    variadicTail = false;
    if (!expectSymbol("(")) return false;

    while (!atEnd() && !checkSymbol(")")) {
        size_t before = m_index;
        parseArgument(arguments, variadicTail);
        if (m_index == before) break;
        if (!matchSymbol(",")) break;
    }
    expectSymbol(")");
    return true;
}

void Parser::parseArgument(std::vector<Argument>& arguments, bool& variadicTail) {
    Argument arg;
    arg.optional = matchKeyword("optional");
    if (checkSymbol("[")) parseExtendedAttributes();

    arg.type = parseType();
    if (matchSymbol("...")) {
        arg.variadic = true;
        variadicTail = true;
    }
    arg.name = parseIdentifier("argument");
    if (matchSymbol("=")) {
        arg.hasDefault = true;
        arg.defaultValue = parseConstantValue();
        arg.optional = true;
    }
    if (!arg.name.empty()) arguments.push_back(std::move(arg));
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

std::string Parser::parseConstantValue() {
    if (current().is(TokenType::Integer) || current().is(TokenType::Decimal) ||
        current().is(TokenType::String)) {
        return advance().text;
    }
    if (checkKeyword("true") || checkKeyword("false")) {
        return advance().text;
    }
    if (current().is(TokenType::Identifier)) {
        std::string value = advance().text;
        while (checkSymbol(".")) {
            advance();
            value += "." + parseIdentifier("constant");
        }
        return value;
    }
    if (matchSymbol("-")) {
        std::string value = "-" + parseConstantValue();
        return value;
    }
    error("expected a constant value");
    return std::string();
}

}  // namespace webidlgen
