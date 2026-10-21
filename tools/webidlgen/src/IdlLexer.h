// Tokenizer for WebIDL source files.
#pragma once

#include <string>
#include <vector>

namespace webidlgen {

enum class TokenType {
    EndOfFile,
    Identifier,
    Integer,
    Decimal,
    String,
    Symbol,
};

struct Token {
    TokenType type = TokenType::EndOfFile;
    std::string text;
    int line = 1;
    int column = 1;

    bool is(TokenType t) const { return type == t; }
    bool isSymbol(const char* s) const { return type == TokenType::Symbol && text == s; }
    bool isKeyword(const char* s) const { return type == TokenType::Identifier && text == s; }
};

class Lexer {
public:
    Lexer(std::string source, std::string filename);

    // Tokenizes the whole input.  Always succeeds; lexical problems are
    // reported through errors().
    void tokenize();

    const std::vector<Token>& tokens() const { return m_tokens; }
    const std::vector<std::string>& errors() const { return m_errors; }
    const std::string& filename() const { return m_filename; }

private:
    void error(const std::string& message, int line, int column);
    char peek(size_t offset = 0) const;
    char advance();
    bool match(char c);
    void skipTrivia();
    void readIdentifier();
    void readNumber();
    void readString(char quote);

    std::string m_source;
    std::string m_filename;
    size_t m_pos = 0;
    int m_line = 1;
    int m_column = 1;
    std::vector<Token> m_tokens;
    std::vector<std::string> m_errors;
};

}  // namespace webidlgen
