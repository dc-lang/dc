#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <lexer.hpp>
#include <stack>
#include <args.hpp>
#include <iostream>

using namespace llvm;

LLVMContext context;
IRBuilder<> builder(context);
Module fmodule("dc", context);
Lexer *g_lexer;

Value *parseExpr(Type *preferred_type, bool rewind = false);

void compilationError(std::string err)
{
  printf("\x1b[1mdcc:\x1b[0m \x1b[1;31mcompilation error:\x1b[0m %s\n", err.c_str());
  exit(1);
}

Type *getTypeFromStr(std::string str)
{
  std::string v = str;

  try
  {
    v.erase(std::remove(v.begin(), v.end(), '*'), v.end());
  }
  catch (std::length_error &e)
  {
    // do nothing
  }

  Type *res = nullptr;
  if (v == "i64")
  {
    res = builder.getInt64Ty();
  }
  else if (v == "i32")
  {
    res = builder.getInt32Ty();
  }
  else if (v == "i8")
  {
    res = builder.getInt8Ty();
  }
  else if (v == "ptr")
  {
    res = builder.getPtrTy();
  }
  else if (v == "void")
  {
    res = builder.getVoidTy();
  }

  int ptrCount = std::count(str.begin(), str.end(), '*');
  if (ptrCount > 0 && res != nullptr)
  {
    for (int i = 0; i < ptrCount; i++)
    {
      res = res->getPointerTo();
    }
  }

  if (res == nullptr)
  {
    compilationError("Unknown type: " + v);
  }
  return res;
}

void catchAndExit(Token &token)
{
  if (token.type == TokenType::END)
  {
    compilationError("unexcepted EOF");
  }
}

typedef struct
{
  Type *llvmType;
  std::string hardcodedName;
  Value *llvmVar;
  bool argVar;
} DCVariable;

typedef struct
{
  FunctionType *fnType;
  Function *fn;
  BasicBlock *fnBlock;
  std::vector<DCVariable> variables;
} DCFunction;

std::vector<DCFunction> functions;

DCVariable *getVarFromFunction(DCFunction &fn, std::string name)
{
  for (DCVariable &var : fn.variables)
  {
    if (var.llvmVar->getName() == name || var.hardcodedName == name)
    {
      return &var;
    }
  }
  compilationError("Unknown variable: " + name);
  return nullptr;
}

std::string parseEscapeSequences(const std::string &input)
{
  std::string output;
  for (size_t i = 0; i < input.length(); ++i)
  {
    if (input[i] == '\\' && i + 1 < input.length())
    {
      // Check the next character for escape sequences
      switch (input[i + 1])
      {
      case 'n':
        output += '\n'; // Newline
        i++;            // Skip the next character
        break;
      case 'r':
        output += '\r'; // Carriage return
        i++;            // Skip the next character
        break;
      case '0':
        output += '\0'; // Null character
        i++;            // Skip the next character
        break;
      case '\\':
        output += '\\'; // Backslash
        i++;            // Skip the next character
        break;
      // Add more escape sequences as needed
      default:
        output += input[i]; // Just add the backslash if not an escape sequence
        break;
      }
    }
    else
    {
      output += input[i]; // Add the current character
    }
  }
  return output;
}

std::vector<std::string> split(std::string s, std::string delimiter)
{
  size_t pos_start = 0, pos_end, delim_len = delimiter.length();
  std::string token;
  std::vector<std::string> res;

  while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos)
  {
    token = s.substr(pos_start, pos_end - pos_start);
    pos_start = pos_end + delim_len;
    res.push_back(token);
  }

  res.push_back(s.substr(pos_start));
  return res;
}

void emitStandardLibrary()
{
  return;
}

Value *perform_LLVM_operation(Value *operand1, Value *operand2, char op)
{
  switch (op)
  {
  case '+':
    return builder.CreateAdd(operand1, operand2);
    break;
  case '-':
    return builder.CreateSub(operand1, operand2);
    break;
  case '*':
    return builder.CreateMul(operand1, operand2);
    break;
  case '/':
    return builder.CreateSDiv(operand1, operand2);
    break;
  }
  return nullptr;
}

template <typename T>
std::vector<T> slice(const std::vector<T> &vec, size_t start, size_t end)
{
  // Ensure start and end are within bounds
  if (start > end || end > vec.size())
  {
    throw std::out_of_range("Invalid slice range");
  }
  return std::vector<T>(vec.begin() + start, vec.begin() + end);
}

Value *evaluate_expression(std::vector<Token> expr, Type *preferred_type)
{
  std::stack<Value *> values;
  std::stack<Token> operators;

  int i = 0;
  Token token = expr.at(i);
  while (i < expr.size())
  {
    token = expr.at(i);
    if (token.type == TokenType::LITERAL)
    {
      values.push(ConstantInt::get(preferred_type, std::stoi(token.value)));
    }
    else if (token.type == TokenType::IDENTIFIER)
    {
      DCVariable *tmp = getVarFromFunction(functions.back(), token.value);
      if (!tmp->argVar)
      {
        values.push(builder.CreateLoad(preferred_type, tmp->llvmVar));
      }
      else
      {
        values.push(tmp->llvmVar);
      }
    }
    else if (token.type == TokenType::OPERATOR)
    {
      while (!operators.empty() && operators.top().type == TokenType::OPERATOR && ((token.value == "+" || token.value == "-" || token.value == "*" || token.value == "/") && (operators.top().value == "*" || operators.top().value == "/")))
      {
        Value *operand2 = values.top();
        values.pop();
        Value *operand1 = values.top();
        values.pop();

        Value *oper_res = perform_LLVM_operation(operand1, operand2, token.value.at(0));
        if (oper_res == nullptr)
        {
          compilationError("Failed to perform LLVM Operation");
        }

        values.push(oper_res);
        operators.pop();
      }
      operators.push(token);
    }
    else if (token.type == TokenType::LPAREN)
    {
      operators.push(token);
    }
    else if (token.type == TokenType::RPAREN)
    {
      while (!operators.empty() && operators.top().type != TokenType::LPAREN)
      {
        Value *operand2 = values.top();
        values.pop();
        Value *operand1 = values.top();
        values.pop();

        Value *oper_res = perform_LLVM_operation(operand1, operand2, operators.top().value.at(0));
        if (oper_res == nullptr)
        {
          compilationError("Failed to perform LLVM Operation");
        }

        values.push(oper_res);
        operators.pop();
      }
      operators.pop();
    }
    i++;
  }

  while (!operators.empty())
  {

    Value *operand2 = values.top();
    values.pop();
    Value *operand1 = values.top();
    values.pop();

    Value *oper_res = perform_LLVM_operation(operand1, operand2, operators.top().value.at(0));
    if (oper_res == nullptr)
    {
      compilationError("Failed to perform LLVM Operation");
    }

    values.push(oper_res);
    operators.pop();
  }

  return values.top();
}

Value *parseExpr(Type *preferred_type, bool rewind)
{
  int old_pos = g_lexer->iterIndex;
  Token token = g_lexer->next();
  std::vector<Token> expr_tokens = {};
  Value *res = nullptr;

  while (token.type != TokenType::END && token.type != TokenType::SEMICOLON)
  {
    expr_tokens.push_back(token);

    token = g_lexer->next();
  }
  if (expr_tokens.size() == 1) // if only single token in entire expression
  {
    token = expr_tokens.at(0);
    std::string eq = token.value;
    if (token.type == TokenType::IDENTIFIER)
    {
      DCVariable *assignToVar = getVarFromFunction(functions.back(), eq);
      Value *tmp = builder.CreateLoad(assignToVar->llvmType, assignToVar->llvmVar);
      res = tmp;
      // builder.CreateStore(tmp, assignVar->llvmVar);
    }
    else if (token.type == TokenType::LITERAL)
    {
      if (eq.at(0) == '\'')
      {
        // builder.CreateStore(builder.getInt8(eq.at(1)), assignVar->llvmVar);
        res = builder.getInt8(eq.at(1));
      }
      else if (eq.at(0) >= '0' && eq.at(0) <= '9')
      {
        Constant *cnst = ConstantInt::get(preferred_type, std::stoi(eq));
        // builder.CreateStore(cnst, );
        res = cnst;
      }
    }
  }
  else
  {
    res = evaluate_expression(expr_tokens, preferred_type);
  }
  if (rewind)
  {
    g_lexer->iterIndex = old_pos;
  }
  return res;
}

void compile(Lexer &lexer, Settings &settings)
{

  fmodule.setDataLayout("e-m:e-i64:64-n8:16:32:64-S128");
  g_lexer = &lexer;
  emitStandardLibrary();

  const DataLayout &dataLayout = fmodule.getDataLayout();

  Token token = lexer.next();
  while (token.type != TokenType::END)
  {
    switch (token.type)
    {
    case TokenType::KEYWORD:
      if (token.value == "extern")
      { // extern instruction
        Token ret = lexer.next();
        catchAndExit(ret);
        Token name = lexer.next();
        catchAndExit(name);

        std::vector<Type *> fnTypes;
        bool vararg = false;
        while (true)
        {
          Token arg = lexer.next();
          if (arg.value == ";")
          {
            break;
          }

          if (arg.value == "vararg")
          {
            vararg = true;
          }
          else
          {
            fnTypes.push_back(getTypeFromStr(arg.value));
          }
        }

        FunctionType *params = FunctionType::get(getTypeFromStr(ret.value), fnTypes, vararg);
        fmodule.getOrInsertFunction(name.value, params);
      }
      else if (token.value == "context")
      {
        token = lexer.next();
        catchAndExit(token);

        if (token.type == TokenType::SEMICOLON)
        {
          functions.pop_back();
          break;
        }

        std::string ctxName = token.value;
        Type *retType = builder.getVoidTy();

        std::vector<Type *> argTypes = {};
        std::vector<std::string> argNames = {};
        while (true)
        {
          token = lexer.next();
          catchAndExit(token);

          if (token.value == "->")
          {
            token = lexer.next();
            catchAndExit(token);
            retType = getTypeFromStr(token.value);
            break;
          }

          if (token.type == TokenType::TYPE)
          {
            argTypes.push_back(getTypeFromStr(token.value));

            token = lexer.next();
            catchAndExit(token);

            argNames.push_back(token.value);
          }
        }

        FunctionType *ctxType = FunctionType::get(retType, argTypes, false);
        Function *ctx = Function::Create(ctxType, Function::ExternalLinkage, ctxName, fmodule);

        BasicBlock *ctxBlock = BasicBlock::Create(context, ctxName + "_blk", ctx);
        builder.SetInsertPoint(ctxBlock);

        functions.push_back({ctxType, ctx, ctxBlock, {}});

        auto fnArgs = ctx->arg_begin();
        Value *arg = fnArgs++;
        for (int i = 0; i < argNames.size(); i++)
        {
          std::string argName = argNames.at(i);
          Type *argType = argTypes.at(i);
          functions.back().variables.push_back({argType, argName, arg, true});
          arg = fnArgs++;
        }
      }
      else if (token.value == "declare")
      {
        token = lexer.next();
        catchAndExit(token);

        Type *varType = getTypeFromStr(token.value);

        token = lexer.next();
        catchAndExit(token);

        std::string varName = token.value;

        Value *var = nullptr;

        token = lexer.next();
        catchAndExit(token);

        var = builder.CreateAlloca(varType, nullptr, varName);

        functions.back().variables.push_back({varType, varName, var, false});
      }
      else if (token.value == "return")
      {
        token = lexer.next();
        catchAndExit(token);
        if (token.type == TokenType::IDENTIFIER)
        {
          // std::string retVarName = token.value;
          // DCVariable *rawVar = getVarFromFunction(functions.back(), retVarName);
          // Value *retVar = builder.CreateLoad(rawVar->llvmType, rawVar->llvmVar);

          // builder.CreateRet(retVar);

          lexer.iterIndex--;
          Value *res = parseExpr(functions.back().fnType->getReturnType());
          builder.CreateRet(res);
        }
        else if (token.type == TokenType::SEMICOLON)
        {
          builder.CreateRet(nullptr);
        }
        else
        {
          Constant *cnst = nullptr;
          if (token.value.at(0) == '\'')
          {
            cnst = ConstantInt::get(functions.back().fnType->getReturnType(), token.value.at(1));
          }
          else
          {
            cnst = ConstantInt::get(functions.back().fnType->getReturnType(), std::stoi(token.value));
          }
          builder.CreateRet(cnst);
        }
      }
      else if (token.value == "assign")
      {
        token = lexer.next();
        catchAndExit(token);

        std::string assignName = token.value;

        DCVariable *assignVar = getVarFromFunction(functions.back(), assignName);

        token = lexer.next();
        catchAndExit(token);

        std::string op = token.value;
        std::string eq = "";

        if (op == "=")
        {
          // token = lexer.next();
          // catchAndExit(token);
          // eq = token.value;

          Value *res = parseExpr(assignVar->llvmType);
          if (res != nullptr)
          {
            builder.CreateStore(res, assignVar->llvmVar);
          }
        }
        else if (op == "->")
        {
          token = lexer.next();
          catchAndExit(token);

          if (token.type != TokenType::IDENTIFIER)
          {
            compilationError("Excepted identifier after -> in assign");
          }
          DCVariable *ptrTo = getVarFromFunction(functions.back(), token.value);

          Type *ptrType = ptrTo->llvmType->getPointerTo();

          Value *ptr = builder.CreatePointerCast(ptrTo->llvmVar, ptrType);

          builder.CreateStore(ptr, assignVar->llvmVar);
        }
        else
        {
          compilationError("Unknown operator: " + op);
        } // TODO: Other operators
      }
      else if (token.value == "deref")
      {
        token = lexer.next();
        catchAndExit(token);

        if (token.type != TokenType::IDENTIFIER)
        {
          compilationError("Excepted identifier after deref");
        }
        std::string toDeref = token.value;

        token = lexer.next();
        catchAndExit(token);

        if (token.type != TokenType::ARROW)
        {
          compilationError("Excepted -> after identifier in deref");
        }

        token = lexer.next();
        catchAndExit(token);

        if (token.type != TokenType::IDENTIFIER)
        {
          compilationError("Excepted identifier after -> in deref");
        }

        std::string dest = token.value;

        DCVariable *toDerefVar = getVarFromFunction(functions.back(), toDeref);
        DCVariable *destVar = getVarFromFunction(functions.back(), dest);

        Value *res = builder.CreateLoad(destVar->llvmType, toDerefVar->llvmVar);

        builder.CreateStore(res, destVar->llvmVar);
      }

      break;
    case TokenType::IDENTIFIER:
      std::string identifier = token.value;
      token = lexer.next();
      catchAndExit(token);

      if (token.type == TokenType::LPAREN) // function call
      {
        std::string fnName = identifier;
        std::vector<Value *> args = {};

        while (true)
        {
          token = lexer.next();
          catchAndExit(token);

          if (token.type == TokenType::SEMICOLON || token.type == TokenType::RPAREN)
          {
            break;
          }

          if (token.type == TokenType::COMMA)
            continue;

          if (token.type == TokenType::STRING_LITERAL)
          {
            std::string text = token.value;
            if (text.at(0) == '"')
            {
              text.erase(text.begin());
              text.erase(text.end() - 1);
            }
            args.push_back(builder.CreateGlobalStringPtr(parseEscapeSequences(text), "", 0U, &fmodule));
          }
          else if (token.type == TokenType::LITERAL)
          {
            if (token.value.at(0) == '\'')
            {
              Constant *cnst = ConstantInt::get(builder.getInt8Ty(), token.value.at(1));
              args.push_back(cnst);
            }
            else if (token.value.at(0) >= '0' && token.value.at(0) <= '9')
            {
              Constant *cnst = ConstantInt::get(builder.getInt32Ty(), std::stoi(token.value));
              args.push_back(cnst);
            }
          }
          else if (token.type == TokenType::IDENTIFIER)
          {
            DCVariable *varFromFn = getVarFromFunction(functions.back(), token.value);
            if (varFromFn->argVar)
            {
              args.push_back(varFromFn->llvmVar);
            }
            else
            {
              Value *var = builder.CreateLoad(varFromFn->llvmType, varFromFn->llvmVar);
              args.push_back(var);
            }
          }
        }

        Function *fn = fmodule.getFunction(fnName);
        if (fn == nullptr)
        {
          compilationError("Undefined reference to " + fnName);
        }

        Value *res = builder.CreateCall(fn, args);

        if (token.type == TokenType::RPAREN)
        {
          token = lexer.next();
          catchAndExit(token);

          if (token.type == TokenType::ARROW)
          {
            token = lexer.next();
            catchAndExit(token);

            DCVariable *tmp = getVarFromFunction(functions.back(), token.value);
            builder.CreateStore(res, tmp->llvmVar);
          }
        }
      }
      break;
    }
    token = lexer.next();
  }

  std::string rawFileName = settings.getFileNameNoExtenstion();
  std::string llcargs = "";
  std::string ccargs = "";
  if (settings.pic == true)
  {
    llcargs = llcargs + "-relocation-model=pic";
  }

  if (!settings.libs.empty())
  {
    for (std::string &lib : split(settings.libs, " "))
    {
      if (!lib.empty() && lib != " ")
      {
        ccargs += "-l" + lib + " ";
      }
    }
  }

  std::error_code EC;
  raw_fd_ostream dest(rawFileName + ".ll", EC);
  if (EC)
  {
    std::cout << "\x1b[1mdcc:\x1b[0m \x1b[1;31mfatal error:\x1b[0m failed to open " << rawFileName + ".ll" << ": " << EC << "\n";
    exit(1);
  }

  fmodule.print(dest, nullptr);
  if (settings.compilation_level == CL_IR)
  {
    return;
  }
  std::string cc_command = "cc " + rawFileName + ".o -o " + rawFileName + " " + ccargs;
  std::string llc_command = "llc " + rawFileName + ".ll -o " + rawFileName + ".s " + llcargs;
  std::string as_command = "as " + rawFileName + ".s -o " + rawFileName + ".o";

  int exitcode = 0;
  exitcode = system(llc_command.c_str());
  if (exitcode != 0)
  {
    std::cout << "\x1b[1mdcc:\x1b[0m \x1b[1;31mfatal error:\x1b[0m failed to compile IR (exit code: " << exitcode << ")\n";
    exit(1);
  }

  if (settings.compilation_level == CL_ASM)
  {
    goto cleanupLevel1;
  }

  exitcode = system(as_command.c_str());
  if (exitcode != 0)
  {
    std::cout << "\x1b[1mdcc:\x1b[0m \x1b[1;31mfatal error:\x1b[0m failed to assemble (exit code: " << exitcode << ")\n";
    exit(1);
  }

  if (settings.compilation_level == CL_OBJ)
  {
    goto cleanupLevel2;
  }

  exitcode = system(cc_command.c_str());
  if (exitcode != 0)
  {
    std::cout << "\x1b[1mdcc:\x1b[0m \x1b[1;31mfatal error:\x1b[0m failed to compile object (exit code: " << exitcode << ")\n";
    std::cout << "\x1b[1mdcc: note:\x1b[0m if you are using an ARM processor, try recompiling with --arm\n";
    exit(1);
  }

cleanupLevel3:
  remove((rawFileName + ".o").c_str());
cleanupLevel2:
  remove((rawFileName + ".s").c_str());
cleanupLevel1:
  remove((rawFileName + ".ll").c_str());
}
