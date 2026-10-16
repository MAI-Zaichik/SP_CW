#include <gtest/gtest.h>
#include "parser/lexer.h"
#include "core/exceptions.h"

static std::vector<Token> lex(const std::string& s) {
    return Lexer(s).tokenize();
}

TEST(Lexer, EmptyInputGivesEof) {
    auto t = lex("");
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].type, TokenType::END_OF_FILE);
}

TEST(Lexer, AlwaysEndsWithEof) {
    auto t = lex("SELECT");
    EXPECT_EQ(t.back().type, TokenType::END_OF_FILE);
}

TEST(Lexer, KeywordUpperAndLower) {
    EXPECT_EQ(lex("SELECT")[0].type, TokenType::K_SELECT);
    EXPECT_EQ(lex("select")[0].type, TokenType::K_SELECT);
}

TEST(Lexer, KeywordKeepsOriginalSpelling) {
    EXPECT_EQ(lex("select")[0].value, "select");
}

TEST(Lexer, MixedCaseKeywordThrows) {
    EXPECT_THROW(lex("SeLeCt"), SyntaxError);
}

TEST(Lexer, IdentifierIsCaseSensitive) {
    auto t = lex("Users");
    EXPECT_EQ(t[0].type, TokenType::IDENT);
    EXPECT_EQ(t[0].value, "Users");
}

TEST(Lexer, EqualityVsAssignment) {
    EXPECT_EQ(lex("==")[0].type, TokenType::OP_EQ);
    EXPECT_EQ(lex("=")[0].type, TokenType::SYM_EQ);
}

TEST(Lexer, ComparisonOperators) {
    EXPECT_EQ(lex("!=")[0].type, TokenType::OP_NEQ);
    EXPECT_EQ(lex("<")[0].type,  TokenType::OP_LT);
    EXPECT_EQ(lex(">")[0].type,  TokenType::OP_GT);
    EXPECT_EQ(lex("<=")[0].type, TokenType::OP_LTE);
    EXPECT_EQ(lex(">=")[0].type, TokenType::OP_GTE);
}

TEST(Lexer, StringLiteralStripsQuotes) {
    auto t = lex("\"hello\"");
    EXPECT_EQ(t[0].type, TokenType::LIT_STRING);
    EXPECT_EQ(t[0].value, "hello");
}

TEST(Lexer, NumberLiteral) {
    auto t = lex("123");
    EXPECT_EQ(t[0].type, TokenType::LIT_INT);
    EXPECT_EQ(t[0].value, "123");
}

TEST(Lexer, MinusIsSeparateToken) {
    auto t = lex("-5");
    EXPECT_EQ(t[0].type, TokenType::SYM_MINUS);
    EXPECT_EQ(t[1].type, TokenType::LIT_INT);
    EXPECT_EQ(t[1].value, "5");
}

TEST(Lexer, UnterminatedStringThrows) {
    EXPECT_THROW(lex("\"abc"), IncompleteInput);
}

TEST(Lexer, BangWithoutEqualsThrows) {
    EXPECT_THROW(lex("!"), SyntaxError);
}

TEST(Lexer, LineCommentSkipped) {
    auto t = lex("-- comment here\nSELECT");
    EXPECT_EQ(t[0].type, TokenType::K_SELECT);
}

TEST(Lexer, SemicolonInCommentIgnored) {
    // ; внутри комментария не должна стать токеном
    auto t = lex("SELECT -- a; b\n;");
    ASSERT_EQ(t.size(), 3u); // SELECT ; EOF
    EXPECT_EQ(t[0].type, TokenType::K_SELECT);
    EXPECT_EQ(t[1].type, TokenType::SYM_SEMICOLON);
}

TEST(Lexer, FullSelectTokenStream) {
    auto t = lex("SELECT * FROM users;");
    ASSERT_EQ(t.size(), 6u);
    EXPECT_EQ(t[0].type, TokenType::K_SELECT);
    EXPECT_EQ(t[1].type, TokenType::SYM_STAR);
    EXPECT_EQ(t[2].type, TokenType::K_FROM);
    EXPECT_EQ(t[3].type, TokenType::IDENT);
    EXPECT_EQ(t[3].value, "users");
    EXPECT_EQ(t[4].type, TokenType::SYM_SEMICOLON);
    EXPECT_EQ(t[5].type, TokenType::END_OF_FILE);
}

TEST(Lexer, DotForDbTableNotation) {
    auto t = lex("db.t");
    EXPECT_EQ(t[0].type, TokenType::IDENT);
    EXPECT_EQ(t[1].type, TokenType::SYM_DOT);
    EXPECT_EQ(t[2].type, TokenType::IDENT);
}

TEST(Lexer, LineNumberTracksNewlines) {
    auto t = lex("SELECT\n\nFROM");
    EXPECT_EQ(t[0].line, 1);
    EXPECT_EQ(t[1].line, 3);
}
