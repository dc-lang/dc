#include <lexer.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>
#include <unordered_map>

Lexer::Lexer(const std::string &src) : source(src), current(0), iterIndex(0), tokens({})
{
  iterIndex--;
  tokenize();
}

void Lexer::tokenize()
{
  tokens.clear();
  while (current < source.size())
  {
    char c = source[current];

    if (isspace(c))
    {
      current++;
    }
    else if (c == '*')
    {
      tokens.push_back(Token(TokenType::OPERATOR, "*"));
      current++;
    }
    else if (isalpha(c) || c == '_' || c == '*')
    {
      tokens.push_back(identifier());
    }
    else if (isdigit(c))
    {
      tokens.push_back(number());
    }
    else if (c == '\'')
    {
      tokens.push_back(character());
    }
    else if (c == '"')
    {
      tokens.push_back(stringLiteral());
    }
    else if (c == ';')
    {
      tokens.push_back(Token(TokenType::SEMICOLON, ";"));
      current++;
    }
    else if (c == '-')
    {
      if (current + 1 < source.size() && source[current + 1] == '>')
      {
        tokens.push_back(Token(TokenType::ARROW, "->"));
        current += 2;
      }
      else
      {
        tokens.push_back(Token(TokenType::OPERATOR, "-"));
        current++;
      }
    }
    else if (c == '+')
    {
      tokens.push_back(Token(TokenType::OPERATOR, "+"));
      current++;
    }
    else if (c == '/')
    {
      tokens.push_back(Token(TokenType::OPERATOR, "/"));
      current++;
    }
    else if (c == '=')
    {
      tokens.push_back(Token(TokenType::OPERATOR, "="));
      current++;
    }
    else if (c == '(')
    {
      tokens.push_back(Token(TokenType::LPAREN, "("));
      current++;
    }
    else if (c == ')')
    {
      tokens.push_back(Token(TokenType::RPAREN, ")"));
      current++;
    }
    else if (c == ',')
    {
      tokens.push_back(Token(TokenType::COMMA, ","));
      current++;
    }
    else if (c == '%')
    {
      tokens.push_back(Token(TokenType::OPERATOR, "%"));
      current++;
    }
    else
    {
      tokens.push_back(Token(TokenType::UNKNOWN, std::string(1, c)));
      current++;
    }
  }
  tokens.push_back(Token(TokenType::END, ""));
}

Token Lexer::next()
{
  iterIndex++;
  try
  {
    return tokens.at(iterIndex);
  }
  catch (std::out_of_range)
  {
    return Token(TokenType::END, "");
  }
}

Token Lexer::identifier()
{
  size_t start = current;
  while (current < source.size() && (isalnum(source[current]) || source[current] == '_' || source[current] == '*'))
  {
    current++;
  }
  std::string value = source.substr(start, current - start);
  if (isKeyword(value))
  {
    return Token(TokenType::KEYWORD, value);
  }
  else if (isType(value))
  {
    return Token(TokenType::TYPE, value, std::count(value.begin(), value.end(), '*'));
  }
  return Token(TokenType::IDENTIFIER, value);
}

Token Lexer::number()
{
  size_t start = current;
  while (current < source.size() && isdigit(source[current]))
  {
    current++;
  }
  return Token(TokenType::LITERAL, source.substr(start, current - start));
}

Token Lexer::character()
{
  size_t start = current++;
  while (current < source.size() && source[current] != '\'')
  {
    current++;
  }
  current++; // Skip the closing quote
  return Token(TokenType::LITERAL, source.substr(start, current - start));
}

Token Lexer::stringLiteral()
{
  size_t start = current++;
  while (current < source.size() && source[current] != '"')
  {
    current++;
  }
  current++; // Skip the closing quote
  return Token(TokenType::STRING_LITERAL, source.substr(start, current - start));
}

bool Lexer::isKeyword(const std::string &value)
{
  static const std::unordered_map<std::string, TokenType> keywords = {
      {"extern", TokenType::KEYWORD},
      {"context", TokenType::KEYWORD},
      {"declare", TokenType::KEYWORD},
      {"assign", TokenType::KEYWORD},
      {"call", TokenType::KEYWORD},
      {"return", TokenType::KEYWORD}};
  return keywords.find(value) != keywords.end();
}

bool Lexer::isType(std::string &value)
{
  std::string v = value;
  try
  {
    v.erase(std::remove(v.begin(), v.end(), '*'), v.end());
  }
  catch (std::length_error &e)
  {
    // do nothing
  }
  static const std::unordered_map<std::string, TokenType> types = {
      {"i64", TokenType::TYPE},
      {"i32", TokenType::TYPE},
      {"i16", TokenType::TYPE},
      {"i8", TokenType::TYPE},
      {"ptr", TokenType::TYPE}};
  return types.find(v) != types.end();
}
