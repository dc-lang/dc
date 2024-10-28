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

#pragma region collapseThis

typedef struct
{
  Type *llvmType;
  std::string hardcodedName;
  Value *llvmVar;
} DCVariable;

typedef struct
{
  BasicBlock *trueBlock;
  BasicBlock *falseBlock;
  BasicBlock *mergeBlock;
  bool elif;
} DCIfStatement;

typedef struct
{
  FunctionType *fnType;
  Function *fn;
  BasicBlock *fnBlock;
  std::vector<DCVariable> variables;
  std::vector<DCIfStatement> ifstatements;
} DCFunction;

std::vector<DCFunction> functions;
std::vector<DCFunction> all_functions;

Value *parseExpr(Type *preferred_type = nullptr, bool rewind = false, std::string stopExprValue = "");

llvm::Value *castValue(llvm::Value *value, llvm::Type *targetType)
{
  // Get the current context
  llvm::LLVMContext &context = builder.getContext();

  // Check if the value is already of the target type
  if (value->getType() == targetType)
  {
    return value; // No cast needed
  }

  // Handle pointer to integer casts and vice versa
  if (llvm::PointerType *ptrType = llvm::dyn_cast<llvm::PointerType>(value->getType()))
  {
    if (targetType->isIntegerTy())
    {
      // Pointer to Integer cast
      return builder.CreatePtrToInt(value, targetType, "ptr_to_int");
    }
  }
  else if (llvm::PointerType *targetPtrType = llvm::dyn_cast<llvm::PointerType>(targetType))
  {
    if (value->getType()->isIntegerTy())
    {
      // Integer to Pointer cast
      return builder.CreateIntToPtr(value, targetPtrType, "int_to_ptr");
    }
  }

  // Perform the cast based on the types
  if (value->getType()->isIntegerTy() && targetType->isIntegerTy())
  {
    // Integer to Integer cast
    return builder.CreateIntCast(value, targetType, true, "int_cast");
  }
  else if (value->getType()->isFloatingPointTy() && targetType->isFloatingPointTy())
  {
    // Float to Float cast
    return builder.CreateFPCast(value, targetType, "fp_cast");
  }
  else if (value->getType()->isIntegerTy() && targetType->isFloatingPointTy())
  {
    // Integer to Float cast
    return builder.CreateSIToFP(value, targetType, "int_to_fp");
  }
  else if (value->getType()->isFloatingPointTy() && targetType->isIntegerTy())
  {
    // Float to Integer cast
    return builder.CreateFPToSI(value, targetType, "fp_to_int");
  }
  else if (value->getType()->isIntegerTy() && targetType->isIntegerTy())
  {
    // Handle cases where the bit sizes differ
    llvm::IntegerType *sourceType = llvm::dyn_cast<llvm::IntegerType>(value->getType());
    llvm::IntegerType *targetIntType = llvm::dyn_cast<llvm::IntegerType>(targetType);

    if (sourceType && targetIntType)
    {
      if (sourceType->getBitWidth() > targetIntType->getBitWidth())
      {
        // Source type has more bits than target type, perform truncation
        return builder.CreateTrunc(value, targetType, "trunc");
      }
      else if (sourceType->getBitWidth() < targetIntType->getBitWidth())
      {
        // Source type has fewer bits than target type, perform extension
        return builder.CreateZExt(value, targetType, "zext"); // Use CreateSExt for signed extension
      }
    }
  }

  // Handle other cases or throw an error
  llvm::errs() << "Unsupported cast from " << *value->getType() << " to " << *targetType << "\n";
  return nullptr;
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

std::string replaceAll(const std::string &str, const std::string &from, const std::string &to)
{
  if (from.empty())
    return str; // Avoid empty substring case

  std::string result = str; // Create a copy of the original string
  size_t start_pos = 0;
  while ((start_pos = result.find(from, start_pos)) != std::string::npos)
  {
    result.replace(start_pos, from.length(), to);
    start_pos += to.length(); // Move past the replaced part
  }
  return result; // Return the modified string
}

std::string deleteDigits(const std::string &str)
{
  std::string res = "";

  for (int i = 0; i < str.length(); i++)
  {
    char c = str.at(i);

    if (c >= '0' && c <= '9')
    {
    }
    else
    {
      res.push_back(c);
    }
  }

  return res;
}

std::string getTypeName(llvm::Type *type)
{
  if (!type)
  {
    return "void";
  }

  std::string typeName;
  llvm::raw_string_ostream rso(typeName);
  type->print(rso);
  return rso.str();
}

std::string mangleCtxName(Type *returnType, std::vector<Type *> fnArgs, std::string name)
{
  if (name == "main")
    return "main";
  std::string moduleId = deleteDigits(replaceAll(fmodule.getModuleIdentifier(), "_", ""));
  std::string fnName = deleteDigits(replaceAll(name, "_", ""));
  // std::string res = "_Z" + std::to_string(fnArgs.size()) + "_" + replaceAll(fmodule.getModuleIdentifier(), "_", "");
  std::string res = "_Z" + std::to_string(fnName.length()) + fnName + std::to_string(moduleId.length()) + moduleId + "_";
  res += getTypeName(returnType);
  res += "_";
  for (Type *type : fnArgs)
  {
    res += getTypeName(type) + "_";
  }
  if (res.back() == '_')
  {
    res.pop_back();
  }
  return res;
}

std::string demangleCtxName(std::string mangled)
{
  if (!mangled.starts_with("_Z"))
  {
    return mangled;
  }

  int nameLen = 0;
  int i = 0;
  for (i = 2; i < mangled.size(); i++)
  {
    char c = mangled.at(i);
    if (c >= '0' && c <= '9')
    {
      nameLen *= 10;
      nameLen += c - '0';
    }
    else
    {
      break;
    }
  }

  std::string fnNameDemangled = "";
  for (; i < mangled.size(); i++)
  {
    char c = mangled.at(i);
    if (c >= '0' && c <= '9')
      break;

    fnNameDemangled.push_back(c);
  }

  return fnNameDemangled;
}

void compilationError(std::string err)
{
  printf(std::string("\x1b[1mdcc:\x1b[0m \x1b[1;31mcompilation error:\n ~" + std::to_string(g_lexer->tokens[g_lexer->iterIndex].line) + " | \x1b[0m %s\n").c_str(), err.c_str());
  exit(1);
}

std::string getMangledName(std::string raw)
{
  if (raw == "main")
    return "main";
  for (DCFunction &fn : all_functions)
  {
    std::string fnName = fn.fn->getName().str();
    std::string current = demangleCtxName(fnName);
    if (current == deleteDigits(replaceAll(raw, "_", "")))
    {
      return fnName;
    }
  }
  return raw;
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
  else if (v == "str")
  {
    res = builder.getInt8Ty()->getPointerTo();
  }

  int ptrCount = std::count(str.begin(), str.end(), '*');
  if (ptrCount > 0 && res != nullptr)
  {
    for (int i = 0; i < ptrCount; i++)
    {
      // res = res->getPointerTo();
      res = PointerType::get(res, 0);
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
      if (token.value.at(0) != '\'')
      {
        values.push(ConstantInt::get(preferred_type, std::stoi(token.value)));
      }
      else
      {
        values.push(ConstantInt::get(preferred_type, token.value.at(1) - '0'));
      }
    }
    else if (token.type == TokenType::IDENTIFIER)
    {
      DCVariable *tmp = getVarFromFunction(functions.back(), token.value);
      values.push(builder.CreateLoad(preferred_type, tmp->llvmVar));
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

Value *parseExpr(Type *preferred_type, bool rewind, std::string stopExprValue)
{
  int old_pos = g_lexer->iterIndex;
  Token token = g_lexer->next();
  std::vector<Token> expr_tokens = {};
  Value *res = nullptr;

  Type *ty = preferred_type;
  if (ty == nullptr)
  {
    ty = builder.getInt32Ty();
  }

  while (token.type != TokenType::END && token.type != TokenType::SEMICOLON && token.value != "==" && token.value != "!=" && token.value != "->" && token.value != ">" && token.value != "<" && token.value != "<=" && token.value != ">=" && token.value != stopExprValue)
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

      Value *tmp = nullptr;
      tmp = builder.CreateLoad(assignToVar->llvmType, assignToVar->llvmVar);
      res = tmp;
      ty = assignToVar->llvmType;
      // builder.CreateStore(tmp, assignVar->llvmVar);
    }
    else if (token.type == TokenType::LITERAL)
    {
      if (eq.at(0) == '\'')
      {
        // builder.CreateStore(builder.getInt8(eq.at(1)), assignVar->llvmVar);
        res = builder.getInt8(eq.at(1));
        ty = builder.getInt8Ty();
      }
      else if (eq.at(0) >= '0' && eq.at(0) <= '9')
      {
        Constant *cnst = ConstantInt::get(ty, std::stoi(eq));
        // builder.CreateStore(cnst, );
        res = cnst;
      }
    }
  }
  else
  {
    res = evaluate_expression(expr_tokens, ty);
  }
  if (rewind)
  {
    g_lexer->iterIndex = old_pos;
  }
  return res;
}

std::string getLabelID();

bool hasBRorRET(BasicBlock &BB)
{
  // Get the last instruction in the BasicBlock
  Instruction *lastInst = BB.getTerminator();
  if (lastInst == nullptr)
    return false;

  // Check if the last instruction is a BranchInst or ReturnInst
  if (isa<BranchInst>(lastInst) || isa<ReturnInst>(lastInst))
  {
    return true; // The BasicBlock ends with BR or RET
  }

  return false; // The BasicBlock does not end with BR or RET
}

Value *cmpExpr(bool isElif = false)
{
  if (isElif)
  {
    builder.SetInsertPoint(functions.back().ifstatements.back().falseBlock);
  }
  Value *LHS = parseExpr();

  Token op = g_lexer->tokens[g_lexer->iterIndex];

  Value *RHS = parseExpr();

  if (op.type != TokenType::OPERATOR)
  {
    compilationError("Non-operator token in IF statement");
  }

  BasicBlock *trueBlock = BasicBlock::Create(context, Twine(getLabelID() + "true"), functions.back().fn);

  BasicBlock *falseBlock = nullptr;
  BasicBlock *mergeBlock = nullptr;

  Value *cmpRes = nullptr;

  if (LHS->getType() != RHS->getType())
  {
    RHS = castValue(RHS, LHS->getType());
  }

  if (op.value == "==")
  {
    cmpRes = builder.CreateICmpEQ(LHS, RHS);
  }
  else if (op.value == "!=")
  {
    cmpRes = builder.CreateICmpNE(LHS, RHS);
  }
  else if (op.value == ">")
  {
    cmpRes = builder.CreateICmpSGT(LHS, RHS);
  }
  else if (op.value == "<")
  {
    cmpRes = builder.CreateICmpSLT(LHS, RHS);
  }
  else if (op.value == ">=")
  {
    cmpRes = builder.CreateICmpSGE(LHS, RHS);
  }
  else if (op.value == "<=")
  {
    cmpRes = builder.CreateICmpSLE(LHS, RHS);
  }

  if (cmpRes == nullptr)
  {
    compilationError("Invalid operator in IF Statement");
  }

  if (!isElif)
  {
    falseBlock = BasicBlock::Create(context, Twine(getLabelID() + "false"), functions.back().fn);
    mergeBlock = BasicBlock::Create(context, Twine(getLabelID() + "merge"), functions.back().fn);
    builder.CreateCondBr(cmpRes, trueBlock, falseBlock);
    builder.SetInsertPoint(trueBlock);
  }
  else
  {
    /*

      when we create an initalizer if we have three blocks:
      true:
      ...
      false:
      ...
      merge:
      basically everything else

      the first thing we wanna do is to copy the merge block addr from previous if/elif to the current elif
      when we create an elif statement we should be inserting in false block of a previous if/elif
      then we create a true and an empty false block
      also if the previous if/elif is true, then we need to insert branch to merge to the true block
    */
    // trueBlock = BasicBlock::Create(context, "", functions.back().fn);
    falseBlock = BasicBlock::Create(context, Twine(getLabelID() + "false"), functions.back().fn);

    mergeBlock = functions.back().ifstatements.back().mergeBlock;

    builder.CreateCondBr(builder.CreateICmpEQ(LHS, RHS), trueBlock, falseBlock);

    // br to merge if previous if/elif is true

    if (!hasBRorRET(*functions.back().ifstatements.back().trueBlock))
    {
      builder.SetInsertPoint(functions.back().ifstatements.back().trueBlock);
      builder.CreateBr(mergeBlock); // merge block is the same all across the if statement, so that it's fine if we use
                                    // the local one
    }

    if (functions.back().ifstatements.back().elif)
    {
      if (!hasBRorRET(*functions.back().ifstatements.back().falseBlock))
      {
        builder.SetInsertPoint(functions.back().ifstatements.back().falseBlock);
        builder.CreateBr(mergeBlock);
      }
    }
    builder.SetInsertPoint(trueBlock);

    /*

      now it should look like this:

      compare
      jump to true1 or false1

      true 1:
      ...
      jump merge


      false 1:
      ...
      compare
      jump to true2 or false2

      true 2:
      ... << Insert point here

      false 2:

      merge:

      == C Pseudocode
      if(...){

      } else {
        if(...){

        } else {

        }
      }
    */
  }
  DCIfStatement ifst;
  ifst.trueBlock = trueBlock;
  ifst.falseBlock = falseBlock;
  ifst.mergeBlock = mergeBlock;
  ifst.elif = isElif;

  functions.back()
      .ifstatements.push_back(ifst);

  return LHS;
}

int label_id = 0;
std::string getLabelID()
{
  label_id++;
  return functions.back().fn->getName().str() + "Label" + std::to_string(label_id);
}
#pragma endregion

void compile(Lexer &lexer, Settings &settings)
{
  fmodule.setModuleIdentifier(replaceAll(settings.output_name, ".", "_"));
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
          verifyFunction(*functions.back().fn);
          functions.pop_back();
          break;
        }

        bool nomangle = false;
        if (token.value == "#nomangle")
        {
          nomangle = true;
          token = lexer.next();
          catchAndExit(token);
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

        if (!nomangle)
        {
          ctxName = mangleCtxName(retType, argTypes, ctxName);
        }
        FunctionType *ctxType = FunctionType::get(retType, argTypes, false);
        Function *ctx = Function::Create(ctxType, Function::ExternalLinkage, ctxName, fmodule);

        BasicBlock *ctxBlock = BasicBlock::Create(context, ctxName + "_blk", ctx);
        builder.SetInsertPoint(ctxBlock);

        functions.push_back({ctxType, ctx, ctxBlock, {}});
        all_functions.push_back({ctxType, ctx, ctxBlock, {}});

        auto fnArgs = ctx->arg_begin();
        Value *arg = fnArgs++;
        for (int i = 0; i < argNames.size(); i++)
        {
          std::string argName = argNames.at(i);
          Type *argType = argTypes.at(i);
          // functions.back().variables.push_back({argType, argName, arg, true});
          Value *var = builder.CreateAlloca(argType);
          builder.CreateStore(arg, var);
          functions.back().variables.push_back({argType, argName, var});
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

        functions.back().variables.push_back({varType, varName, var});
      }
      else if (token.value == "return")
      {
        token = lexer.next();
        catchAndExit(token);
        if (token.type == TokenType::SEMICOLON)
        {
          builder.CreateRet(nullptr);
        }
        else
        {
          lexer.iterIndex--;
          Value *res = parseExpr(functions.back().fnType->getReturnType());
          if (res->getType() != functions.back().fnType->getReturnType())
          {
            // res = builder.CreateCast(Instruction::CastOps::BitCast, res, functions.back().fnType->getReturnType());
            // res = builder.CreateBitCast(res, functions.back().fnType->getReturnType());
            res = castValue(res, functions.back().fnType->getReturnType());
          }
          builder.CreateRet(res);
        }
      }
      else if (token.value == "assign")
      {
        token = lexer.next();
        catchAndExit(token);

        Type *strongType = nullptr;
        bool ptrAssign = false;
        if (token.type == TokenType::TYPE)
        {
          if (token.value == "ptr")
          {
            ptrAssign = true;
            token = lexer.next();
            catchAndExit(token);

            if (token.type != TokenType::TYPE)
            {
              compilationError("Explicit pointer assignment needs to have a type after ptr");
            }
          }
          strongType = getTypeFromStr(token.value);
          token = lexer.next();
          catchAndExit(token);
        }

        std::string assignName = token.value;

        DCVariable *assignVar = getVarFromFunction(functions.back(), assignName);
        if (strongType == nullptr)
        {
          strongType = assignVar->llvmType;
        }

        token = lexer.next();
        catchAndExit(token);

        std::string op = token.value;
        std::string eq = "";

        if (op == "=")
        {
          // token = lexer.next();
          // catchAndExit(token);
          // eq = token.value;

          Value *res = parseExpr(strongType);
          if (res != nullptr)
          {
            if (!ptrAssign)
            {
              builder.CreateStore(res, assignVar->llvmVar);
            }
            else
            {
              builder.CreateStore(res, builder.CreateLoad(builder.getPtrTy(), assignVar->llvmVar));
            }
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

          // Value *ptr = builder.CreatePointerCast(ptrTo->llvmVar, ptrType);
          Value *ptr = builder.CreateBitCast(ptrTo->llvmVar, ptrType);

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

        Value *res = builder.CreateLoad(destVar->llvmType, builder.CreateLoad(toDerefVar->llvmType, toDerefVar->llvmVar));

        builder.CreateStore(res, destVar->llvmVar);
      }
      else if (token.value == "if")
      {
        Value *res = cmpExpr();
      }
      else if (token.value == "else")
      {
        if (!hasBRorRET(*functions.back().ifstatements.back().mergeBlock))
        {
          builder.CreateBr(functions.back().ifstatements.back().mergeBlock);
        }
        builder.SetInsertPoint(functions.back().ifstatements.back().falseBlock);
      }
      else if (token.value == "elif")
      {
        Value *res = cmpExpr(true);
      }
      else if (token.value == "fi")
      {
        if (!hasBRorRET(*functions.back().ifstatements.back().mergeBlock))
        {
          builder.CreateBr(functions.back().ifstatements.back().mergeBlock);
        }
        builder.SetInsertPoint(functions.back().ifstatements.back().falseBlock);
        if (!hasBRorRET(*functions.back().ifstatements.back().falseBlock))
        {
          builder.CreateBr(functions.back().ifstatements.back().mergeBlock);
        }

        builder.SetInsertPoint(functions.back().ifstatements.back().mergeBlock);
        // functions.back().ifstatements.pop_back();

        functions.back().ifstatements.back().mergeBlock->moveAfter(&functions.back().fn->back());

        BasicBlock *mergeBlock = functions.back().ifstatements.back().mergeBlock;

        auto newEnd = std::remove_if(functions.back().ifstatements.begin(), functions.back().ifstatements.end(),
                                     [mergeBlock](DCIfStatement &s)
                                     {
                                       return s.mergeBlock == mergeBlock;
                                     });

        functions.back().ifstatements.erase(newEnd, functions.back().ifstatements.end());

        if (functions.back().ifstatements.size() > 0)
        {
          builder.SetInsertPoint(mergeBlock);
          builder.CreateBr(functions.back().ifstatements.back().mergeBlock);
        }
      }
      else if (token.value == "array")
      {
        token = lexer.next();
        catchAndExit(token);

        if (token.type != TokenType::IDENTIFIER)
          compilationError("Excepted identifier after keyword array");

        DCVariable *arrayVar = getVarFromFunction(functions.back(), token.value);

        Value *index = parseExpr(nullptr, false, "=");

        Value *res = builder.CreateGEP(arrayVar->llvmType, builder.CreateLoad(arrayVar->llvmType, arrayVar->llvmVar), index);

        token = lexer.tokens[lexer.iterIndex];

        if (token.type == TokenType::ARROW)
        {
          token = lexer.next();
          catchAndExit(token);
          if (token.type != TokenType::IDENTIFIER)
            compilationError("Excepted identifier in array after ->");

          DCVariable *storeVar = getVarFromFunction(functions.back(), token.value);

          builder.CreateStore(builder.CreateLoad(storeVar->llvmType, res), storeVar->llvmVar);
        }
        else if (token.type == TokenType::OPERATOR)
        {
          Value *storeVar = parseExpr();
          builder.CreateStore(storeVar, res);
        }
        else
        {
          compilationError("Unsupported token in array after identifier");
        }
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

            Value *var = builder.CreateLoad(varFromFn->llvmType, varFromFn->llvmVar);
            args.push_back(var);
          }
        }

        std::string mangled = getMangledName(fnName);
        Function *fn = fmodule.getFunction(mangled);
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

  std::string rawFileName = settings.output_name;
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

  verifyModule(fmodule);

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
  std::string cc_command = "cc " + rawFileName + ".o -o " + rawFileName + " -g " + ccargs;
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
