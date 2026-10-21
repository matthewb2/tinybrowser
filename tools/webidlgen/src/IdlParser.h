// Recursive descent parser for WebIDL.
#pragma once

#include <string>
#include <vector>

#include "IdlAst.h"
#include "IdlLexer.h"

namespace webidlgen {

class Parser {
public:
    Parser(std::string source, std::string filename);

    // Parses the whole input into `db`.  Returns false when at least one
    // fatal error was reported.
    bool parse(Database& db);

    const std::vector<std::string>& errors() const { return m_errors; }

private:
    // --- token helpers ---
    const Token& current() const { return m_tokens[m_index]; }
    const Token& lookahead(size_t offset = 1) const;
    bool atEnd() const { return current().is(TokenType::EndOfFile); }
    Token advance();
    bool checkSymbol(const char* s) const { return current().isSymbol(s); }
    bool checkKeyword(const char* s) const { return current().isKeyword(s); }
    bool matchSymbol(const char* s);
    bool matchKeyword(const char* s);
    bool expectSymbol(const char* s);
    bool expectKeyword(const char* s);
    void error(const std::string& message);
    void errorAt(const Token& token, const std::string& message);

    // --- grammar ---
    void parseDefinitions(Database& db);
    void parseInterface(Database& db, bool partial, bool isMixin,
                        std::vector<ExtendedAttribute> extended);
    void parseDictionary(Database& db, bool partial);
    void parseEnum(Database& db, bool partial);
    void parseCallback(Database& db);
    void parseTypedef(Database& db);
    void parseNamespace(Database& db);

    void parseMembers(Interface& interface);
    void parseMembers(Dictionary& dictionary);
    void parseMember(Interface& interface);
    void parseDictionaryMember(Dictionary& dictionary);

    bool parseMemberIsStatic();
    Member parseAttributeMember(bool isStatic);
    Member parseOperationMember(bool isStatic, bool isAsync);
    Member parseConstantMember();

    Type parseType();
    Type parseTypeBase();
    Type parseGenericType(const std::string& name);
    bool parseArguments(std::vector<Argument>& arguments, bool& variadicTail);
    void parseArgument(std::vector<Argument>& arguments, bool& variadicTail);
    std::string parseConstantValue();
    std::vector<ExtendedAttribute> parseExtendedAttributes();
    std::string parseIdentifier(const char* what);

    std::vector<Token> m_tokens;
    size_t m_index = 0;
    std::string m_filename;
    std::vector<std::string> m_errors;
};

}  // namespace webidlgen
