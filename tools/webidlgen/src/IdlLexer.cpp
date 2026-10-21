#include "IdlLexer.h"

#include <cctype>
#include <cstring>
#include <utility>

namespace webidlgen {

Lexer::Lexer(std::string source, std::string filename)
    : m_source(std::move(source)), m_filename(std::move(filename)) {}

void Lexer::error(const std::string& message, int line, int column) {
    m_errors.push_back(m_filename + ":" + std::to_string(line) + ":" +
                       std::to_string(column) + ": " + message);
}

char Lexer::peek(size_t offset) const {
    size_t index = m_pos + offset;
    return index < m_source.size() ? m_source[index] : '\0';
}

char Lexer::advance() {
    if (m_pos >= m_source.size()) return '\0';
    char c = m_source[m_pos++];
    if (c == '\n') {
        ++m_line;
        m_column = 1;
    } else {
        ++m_column;
    }
    return c;
}

bool Lexer::match(char c) {
    if (peek() != c) return false;
    advance();
    return true;
}

void Lexer::skipTrivia() {
    for (;;) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
            continue;
        }
        if (c == '/' && peek(1) == '/') {
            while (peek() != '\0' && peek() != '\n') advance();
            continue;
        }
        if (c == '/' && peek(1) == '*') {
            int startLine = m_line;
            int startColumn = m_column;
            advance();
            advance();
            bool closed = false;
            while (peek() != '\0') {
                if (peek() == '*' && peek(1) == '/') {
                    advance();
                    advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) error("unterminated block comment", startLine, startColumn);
            continue;
        }
        return;
    }
}

void Lexer::readIdentifier() {
    Token token;
    token.type = TokenType::Identifier;
    token.line = m_line;
    token.column = m_column;

    // WebIDL allows '-' inside identifiers (e.g. `font-size`), but a '-' is
    // only part of the identifier when it is followed by a letter or '_'.
    while (std::isalpha(static_cast<unsigned char>(peek())) ||
           peek() == '_') {
        token.text.push_back(advance());
    }
    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_' ||
           (peek() == '-' &&
            (std::isalpha(static_cast<unsigned char>(peek(1))) || peek(1) == '_'))) {
        token.text.push_back(advance());
    }
    m_tokens.push_back(std::move(token));
}

void Lexer::readNumber() {
    Token token;
    token.type = TokenType::Integer;
    token.line = m_line;
    token.column = m_column;

    while (std::isdigit(static_cast<unsigned char>(peek()))) {
        token.text.push_back(advance());
    }

    bool isDecimal = false;
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        isDecimal = true;
        token.text.push_back(advance());
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            token.text.push_back(advance());
        }
    }
    if (peek() == 'e' || peek() == 'E') {
        size_t save = m_pos;
        int saveLine = m_line;
        int saveColumn = m_column;
        std::string exponent;
        exponent.push_back(advance());
        if (peek() == '+' || peek() == '-') exponent.push_back(advance());
        if (std::isdigit(static_cast<unsigned char>(peek()))) {
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                exponent.push_back(advance());
            }
            token.text += exponent;
            isDecimal = true;
        } else {
            m_pos = save;
            m_line = saveLine;
            m_column = saveColumn;
        }
    }

    token.type = isDecimal ? TokenType::Decimal : TokenType::Integer;
    m_tokens.push_back(std::move(token));
}

void Lexer::readString(char quote) {
    Token token;
    token.type = TokenType::String;
    token.line = m_line;
    token.column = m_column;
    advance();  // opening quote

    while (peek() != '\0' && peek() != quote) {
        char c = advance();
        if (c == '\\' && peek() != '\0') {
            char escaped = advance();
            switch (escaped) {
                case 'n': token.text.push_back('\n'); break;
                case 't': token.text.push_back('\t'); break;
                case 'r': token.text.push_back('\r'); break;
                default: token.text.push_back(escaped); break;
            }
            continue;
        }
        token.text.push_back(c);
    }

    if (peek() != quote) {
        error("unterminated string literal", token.line, token.column);
    } else {
        advance();
    }
    m_tokens.push_back(std::move(token));
}

void Lexer::tokenize() {
    for (;;) {
        skipTrivia();
        if (m_pos >= m_source.size()) break;

        char c = peek();
        int line = m_line;
        int column = m_column;

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            readIdentifier();
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            readNumber();
            continue;
        }

        if (c == '"' || c == '\'') {
            readString(c);
            continue;
        }

        if (c == '.' && peek(1) == '.' && peek(2) == '.') {
            advance();
            advance();
            advance();
            Token token;
            token.type = TokenType::Symbol;
            token.text = "...";
            token.line = line;
            token.column = column;
            m_tokens.push_back(std::move(token));
            continue;
        }

        if (std::strchr("{}()[];,:=<>?", c) != nullptr) {
            advance();
            Token token;
            token.type = TokenType::Symbol;
            token.text = std::string(1, c);
            token.line = line;
            token.column = column;
            m_tokens.push_back(std::move(token));
            continue;
        }

        error(std::string("unexpected character '") + c + "'", line, column);
        advance();
    }

    Token eof;
    eof.type = TokenType::EndOfFile;
    eof.line = m_line;
    eof.column = m_column;
    m_tokens.push_back(std::move(eof));
}

}  // namespace webidlgen
