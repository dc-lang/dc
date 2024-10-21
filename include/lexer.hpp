#if !defined(LEXER_H)
#define LEXER_H

#include <string>
#include <cstdint>
#include <vector>

enum class TokenType
{
	KEYWORD,
	TYPE,
	IDENTIFIER,
	LITERAL,
	OPERATOR,
	SEMICOLON,
	ARROW,
	STRING_LITERAL,
	UNKNOWN,
	END
};

typedef struct Token
{
	TokenType type;
	std::string value;
	int ptrCount;

	Token(TokenType t, const std::string &v, int p = 0) : type(t), value(v), ptrCount(p) {}
} Token;

class Lexer
{
public:
	Lexer(const std::string &src);

	std::vector<Token> tokens;
	void tokenize();
	Token next();
	int iterIndex;

private:
	std::string source;
	size_t current;

	Token identifier();

	Token number();

	Token character();

	Token stringLiteral();

	bool isKeyword(const std::string &value);
	bool isType(std::string &value);
};

#endif // LEXER_H
