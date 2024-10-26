#include <lexer.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>
#include <unordered_map>

Lexer::Lexer(const std::string &src, int startingLine) : source(src), current(0), iterIndex(0), tokens({}), line(0)
{
  iterIndex--;
  line -= startingLine;
  tokenize();
}

void Lexer::tokenize()
{
  tokens.clear();
  while (current < source.size())
  {
    char c = source[current];

    if (isspace(c) && c != '\n')
    {
      current++;
    }
    else if (c == '*')
    {
      tokens.push_back(Token(TokenType::OPERATOR, "*", 0, line));
      current++;
    }
    else if (isalpha(c) || c == '_' || c == '*' || c == '#')
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
      tokens.push_back(Token(TokenType::SEMICOLON, ";", 0, line));
      current++;
    }
    else if (c == '-')
    {
      if (current + 1 < source.size() && source[current + 1] == '>')
      {
        tokens.push_back(Token(TokenType::ARROW, "->", 0, line));
        current += 2;
      }
      else
      {
        tokens.push_back(Token(TokenType::OPERATOR, "-", 0, line));
        current++;
      }
    }
    else if (c == '+')
    {
      tokens.push_back(Token(TokenType::OPERATOR, "+", 0, line));
      current++;
    }
    else if (c == '/')
    {
      tokens.push_back(Token(TokenType::OPERATOR, "/", 0, line));
      current++;
    }
    else if (c == '=')
    {
      if (current + 1 < source.size() && source[current + 1] == '=')
      {
        tokens.push_back(Token(TokenType::OPERATOR, "==", 0, line));
        current += 2;
      }
      else
      {
        tokens.push_back(Token(TokenType::OPERATOR, "=", 0, line));
        current++;
      }
    }
    else if (c == '<')
    {
      if (current + 1 < source.size() && source[current + 1] == '=')
      {
        tokens.push_back(Token(TokenType::OPERATOR, "<=", 0, line));
        current += 2;
      }
      else
      {
        tokens.push_back(Token(TokenType::OPERATOR, "<", 0, line));
        current++;
      }
    }
    else if (c == '>')
    {
      if (current + 1 < source.size() && source[current + 1] == '=')
      {
        tokens.push_back(Token(TokenType::OPERATOR, ">=", 0, line));
        current += 2;
      }
      else
      {
        tokens.push_back(Token(TokenType::OPERATOR, ">", 0, line));
        current++;
      }
    }
    else if (c == '!')
    {
      if (current + 1 < source.size() && source[current + 1] == '=')
      {
        tokens.push_back(Token(TokenType::OPERATOR, "!=", 0, line));
        current += 2;
      }
    }
    else if (c == '(')
    {
      tokens.push_back(Token(TokenType::LPAREN, "(", 0, line));
      current++;
    }
    else if (c == ')')
    {
      tokens.push_back(Token(TokenType::RPAREN, ")", 0, line));
      current++;
    }
    else if (c == ',')
    {
      tokens.push_back(Token(TokenType::COMMA, ",", 0, line));
      current++;
    }
    else if (c == '%')
    {
      tokens.push_back(Token(TokenType::OPERATOR, "%", 0, line));
      current++;
    }
    else if (c == '\n')
    {
      line++;
      current++;
    }
    else
    {
      tokens.push_back(Token(TokenType::UNKNOWN, std::string(1, c), 0, line));
      current++;
    }
  }
  tokens.push_back(Token(TokenType::END, "", 0, line));
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
    return Token(TokenType::END, "", 0, line);
  }
}

Token Lexer::identifier()
{
  size_t start = current;
  while (current < source.size() && (isalnum(source[current]) || source[current] == '_' || source[current] == '*' || source[current] == '#'))
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
    return Token(TokenType::TYPE, value, std::count(value.begin(), value.end(), '*'), line);
  }
  return Token(TokenType::IDENTIFIER, value, 0, line);
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
  return Token(TokenType::LITERAL, source.substr(start, current - start), 0, line);
}

Token Lexer::stringLiteral()
{
  size_t start = current++;
  while (current < source.size() && source[current] != '"')
  {
    current++;
  }
  current++; // Skip the closing quote
  return Token(TokenType::STRING_LITERAL, source.substr(start, current - start), 0, line);
}

bool Lexer::isKeyword(const std::string &value)
{
  static const std::unordered_map<std::string, TokenType> keywords = {
      {"extern", TokenType::KEYWORD},
      {"context", TokenType::KEYWORD},
      {"declare", TokenType::KEYWORD},
      {"assign", TokenType::KEYWORD},
      {"deref", TokenType::KEYWORD},
      {"if", TokenType::KEYWORD},
      {"fi", TokenType::KEYWORD},
      {"else", TokenType::KEYWORD},
      {"elif", TokenType::KEYWORD},
      {"array", TokenType::KEYWORD},
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
      {"str", TokenType::TYPE},
      {"ptr", TokenType::TYPE}};
  return types.find(v) != types.end();
}
