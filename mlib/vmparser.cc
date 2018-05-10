/* vmparser.cc (updated on 2017/09/08)
 * Copyright (C) 2017 renny1398.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "vmparser.h"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cassert>

namespace {

// empty stream buffer
class NullBuffer : public std::streambuf {
public:
  int overflow(int c) { return c; }
};

void GetStringFromVMData(const std::vector<char16_t>& vmdata, int offset,
                         std::u16string* out) {
  auto it = vmdata.cbegin();
  it += offset >> 1;
  out->clear();
  for (; it != vmdata.end(); ++it) {
    if (*it == u'\0') {
      break;
    }
    if (*it == u'\n') {
      break;
    }
    out->push_back(*it);
  }
}

} // namespace

template<> void std::swap(mlib::StackVariable& a, mlib::StackVariable& b) {
  a.swap(b);
}

namespace mlib {

////////////////////////////////////////////////////////////////////////
// Numerical Expression
////////////////////////////////////////////////////////////////////////

std::string& NumericalExpression::PutInParenthesesIfNeeded(std::string* s) const {
  assert(s != nullptr);
  if (IsPolynomial()) {
    s->insert(0, 1, '(');
    s->push_back(')');
  }
  return *s;
}

std::string NumericalExpression::MonomialToString(const char* oper,
                                                  const std::unique_ptr<NumericalExpression>& p_term) {
  std::string ret(oper);
  std::string term_str(p_term->ToString());
  ret.append(p_term->PutInParenthesesIfNeeded(&term_str));
  return ret;
}

std::string NumericalExpression::PolynomialToString(const std::string& oper,
                                                    const std::unique_ptr<NumericalExpression>& p_term1,
                                                    const std::unique_ptr<NumericalExpression>& p_term2) {
  std::string ret(p_term1->ToString());
  std::string term2_str(p_term2->ToString());
  p_term1->PutInParenthesesIfNeeded(&ret);
  p_term2->PutInParenthesesIfNeeded(&term2_str);
  ret.append(1, ' ').append(oper).append(1, ' ').append(term2_str);
  return ret;
}

class ImmediateExpression : public NumericalExpression {
public:
  explicit ImmediateExpression(int value)
    : value_(value) {}
  bool IsPolynomial() const { return false; }
  int Evaluate() const { return value_; }
  std::string ToString() const {
    return std::to_string(value_);
  }
  NumericalExpression* Clone() const {
    return new ImmediateExpression(value_);
  }
private:
  int value_;
};

class VariableExpression : public NumericalExpression {
public:
  VariableExpression(Exec* p_exec, int var_offset)
    : p_exec_(p_exec), var_offset_(var_offset) {}
  bool IsPolynomial() const { return false; }
  int Evaluate() const {
    return p_exec_->GetVariableValue(var_offset_);
  }
  std::string ToString() const {
    return p_exec_->GetVariableName(var_offset_);
  }
  NumericalExpression* Clone() const {
    return new VariableExpression(p_exec_, var_offset_);
  }
private:
  Exec* p_exec_;
  int var_offset_;
};

class StringExpression : public NumericalExpression {
public:
  StringExpression(int value, const std::string& str)
    : value_(value), str_(str) {}
  bool IsPolynomial() const { return false; }
  int Evaluate() const {
    return value_;
  }
  std::string ToString() const {
    std::string ret(1, '"');
    ret.append(str_).append(1, '"');
    return ret;
  }
  NumericalExpression* Clone() const {
    return new StringExpression(value_, str_);
  }
private:
  int value_;
  std::string str_;
};

#define DECLARE_UNARY_OPERATION(name, oper) \
  class name##Expression : public NumericalExpression {\
  public:\
    explicit name##Expression(std::unique_ptr<NumericalExpression>&& p_exp) noexcept\
      : p_term_(std::move(p_exp)) {}\
    bool IsPolynomial() const { return true; }\
    int Evaluate() const {\
      return oper(p_term_->Evaluate());\
    }\
    std::string ToString() const {\
      return MonomialToString(#oper, p_term_);\
    }\
    NumericalExpression* Clone() const {\
      std::unique_ptr<NumericalExpression> p_cloned_term(p_term_->Clone());\
      return new name##Expression(std::move(p_cloned_term));\
    }\
  NumericalExpression* Optimize() {\
    NumericalExpression* p = p_term_.get();\
    NumericalExpression* p_tmp = p->Optimize();\
    if (p_tmp != nullptr) {\
      p = p_tmp;\
      p_term_.reset(p);\
    }\
    if (typeid(*p) == typeid(ImmediateExpression)) {\
      p_tmp = new ImmediateExpression(Evaluate());\
      return p_tmp;\
    }\
    return nullptr;\
  }\
  private:\
    std::unique_ptr<NumericalExpression> p_term_;\
  }

#define DECLARE_BINARY_OPERATION(name, oper) \
  class name##Expression : public NumericalExpression {\
  public:\
    name##Expression(std::unique_ptr<NumericalExpression>&& p_term1,\
                     std::unique_ptr<NumericalExpression>&& p_term2) noexcept\
      : p_term1_(std::move(p_term1)), p_term2_(std::move(p_term2)) {}\
    bool IsPolynomial() const { return true; }\
    int Evaluate() const {\
      return p_term1_->Evaluate() oper p_term2_->Evaluate();\
    }\
    std::string ToString() const {\
      return PolynomialToString(#oper, p_term1_, p_term2_);\
    }\
  NumericalExpression* Clone() const {\
    std::unique_ptr<NumericalExpression> p_cloned_term1(p_term1_->Clone());\
    std::unique_ptr<NumericalExpression> p_cloned_term2(p_term2_->Clone());\
    return new name##Expression(std::move(p_cloned_term1), std::move(p_cloned_term2));\
  }\
  NumericalExpression* Optimize() {\
    NumericalExpression* p1 = p_term1_.get();\
    NumericalExpression* p2 = p_term2_.get();\
    NumericalExpression* p_tmp = p1->Optimize();\
    if (p_tmp != nullptr) {\
      p1 = p_tmp;\
      p_term1_.reset(p1);\
    }\
    p_tmp = p2->Optimize();\
    if (p_tmp != nullptr) {\
      p2 = p_tmp;\
      p_term2_.reset(p2);\
    }\
    p_tmp = nullptr;\
    if (typeid(*p1) == typeid(ImmediateExpression) && typeid(*p2) == typeid(ImmediateExpression)) {\
      p_tmp = new ImmediateExpression(Evaluate());\
    } else if (::strcmp(#oper, "+") == 0 || ::strcmp(#oper, "|") == 0) {\
      if (typeid(*p1) == typeid(ImmediateExpression) && p1->Evaluate() == 0) {\
        p_tmp = p2->Clone();\
      } else if (typeid(*p1) == typeid(ImmediateExpression) && p2->Evaluate() == 0) {\
        p_tmp = p1->Clone();\
      }\
    } else if (::strcmp(#oper, "*") == 0) {\
      if (typeid(*p1) == typeid(ImmediateExpression) && p1->Evaluate() == 1) {\
        p_tmp = p2->Clone();\
      } else if (typeid(*p1) == typeid(ImmediateExpression) && p2->Evaluate() == 1) {\
        p_tmp = p1->Clone();\
      }\
    }\
    return p_tmp;\
  }\
  private:\
    std::unique_ptr<NumericalExpression> p_term1_;\
    std::unique_ptr<NumericalExpression> p_term2_;\
  }

#define DECLARE_BINARY_OPERATION_BOOL(__name__, oper) \
  class __name__##Expression : public NumericalExpression {\
  public:\
    __name__##Expression(std::unique_ptr<NumericalExpression>&& p_term1,\
                     std::unique_ptr<NumericalExpression>&& p_term2) noexcept\
      : p_term1_(std::move(p_term1)), p_term2_(std::move(p_term2)) {}\
    bool IsBoolean() const { return true; }\
    bool IsPolynomial() const { return true; }\
    int Evaluate() const {\
      return (p_term1_->Evaluate() oper p_term2_->Evaluate()) ? 1 : 0;\
    }\
    std::string ToString() const {\
      return PolynomialToString(#oper, p_term1_, p_term2_);\
    }\
    NumericalExpression* Clone() const {\
      std::unique_ptr<NumericalExpression> p_cloned_term1(p_term1_->Clone());\
      std::unique_ptr<NumericalExpression> p_cloned_term2(p_term2_->Clone());\
      return new __name__##Expression(std::move(p_cloned_term1), std::move(p_cloned_term2));\
    }\
    NumericalExpression* Optimize() {\
      NumericalExpression* p1 = p_term1_.get();\
      NumericalExpression* p2 = p_term2_.get();\
      NumericalExpression* p_tmp = p1->Optimize();\
      if (p_tmp != nullptr) {\
        p1 = p_tmp;\
        p_term1_.reset(p1);\
      }\
      p_tmp = p2->Optimize();\
      if (p_tmp != nullptr) {\
        p2 = p_tmp;\
        p_term2_.reset(p2);\
      }\
      if (typeid(*p1) == typeid(ImmediateExpression) &&\
          typeid(*p2) == typeid(ImmediateExpression)) {\
        /* TODO: BooleanImmediateExpression */ \
        p_tmp = new ImmediateExpression(Evaluate());\
        return p_tmp;\
      }\
      return nullptr;\
    }\
  private:\
    std::unique_ptr<NumericalExpression> p_term1_;\
    std::unique_ptr<NumericalExpression> p_term2_;\
  }

#define DECLARE_PREFIX_UNARY_OPERATION(name, oper) \
  class name##Expression : public NumericalExpression {\
  public:\
    explicit name##Expression(std::unique_ptr<NumericalExpression>&& p_exp)\
      : p_term_(std::move(p_exp)) {}\
    bool IsPolynomial() const { return true; }\
    int Evaluate() const {\
      return p_term_->Evaluate() oper 1;\
    }\
    std::string ToString() const {\
      std::unique_ptr<NumericalExpression> p_imm_term(new ImmediateExpression(1));\
      return PolynomialToString(#oper, p_term_, p_imm_term);\
    }\
    NumericalExpression* Clone() const {\
      std::unique_ptr<NumericalExpression> p_cloned_term(p_term_->Clone());\
      return new name##Expression(std::move(p_cloned_term));\
    }\
  private:\
    std::unique_ptr<NumericalExpression> p_term_;\
  }

DECLARE_UNARY_OPERATION(Neg, -);
DECLARE_BINARY_OPERATION(Add, +);
DECLARE_BINARY_OPERATION(Sub, -);
DECLARE_BINARY_OPERATION(Mul, *);
DECLARE_BINARY_OPERATION(Div, /);
DECLARE_BINARY_OPERATION(Mod, %);
DECLARE_BINARY_OPERATION(And, &);
DECLARE_BINARY_OPERATION(Or, |);
DECLARE_BINARY_OPERATION(Xor, ^);
DECLARE_UNARY_OPERATION(Not, ~);

class BoolExpression : public NumericalExpression {
public:
  BoolExpression(std::unique_ptr<NumericalExpression>&& p_exp)
    : p_term_(std::move(p_exp)) {}
  bool IsBoolean() const { return true; }
  bool IsPolynomial() const { return false; }
  int Evaluate() const {
    return (p_term_->Evaluate() != 0) ? 1 : 0;
  }
  std::string ToString() const {
    std::unique_ptr<NumericalExpression> p_imm_term(new ImmediateExpression(0));
    return PolynomialToString("!=", p_term_, p_imm_term);
  }
  NumericalExpression* Clone() const {
    std::unique_ptr<NumericalExpression> p_cloned_term(p_term_->Clone());
    return new BoolExpression(std::move(p_cloned_term));
  }
private:
  std::unique_ptr<NumericalExpression> p_term_;
};

DECLARE_BINARY_OPERATION_BOOL(BoolAnd, &&); // TODO: multi &&
DECLARE_BINARY_OPERATION_BOOL(BoolOr, ||);

class BoolNotExpression : public NumericalExpression {
public:
  BoolNotExpression(std::unique_ptr<NumericalExpression>&& p_exp)
    : p_term_(std::move(p_exp)) {}
  bool IsBoolean() const { return true; }
  bool IsPolynomial() const { return false; }
  int Evaluate() const {
    return (p_term_->Evaluate() == 0) ? 1 : 0;
  }
  std::string ToString() const {
    std::unique_ptr<NumericalExpression> p_imm_term(new ImmediateExpression(0));
    return PolynomialToString("==", p_term_, p_imm_term);
  }
  NumericalExpression* Clone() const {
    std::unique_ptr<NumericalExpression> p_cloned_term(p_term_->Clone());
    return new BoolExpression(std::move(p_cloned_term));
  }
  virtual NumericalExpression* Optimize() {
    NumericalExpression* p = p_term_.get();
    NumericalExpression* p_tmp = p->Optimize();
    if (p_tmp != nullptr) {
      p = p_tmp;
      p_term_.reset(p);
    }
    if (typeid(*p) == typeid(ImmediateExpression)) {
      /* TODO: BooleanImmediateExpression */
      p_tmp = new ImmediateExpression(Evaluate());
      return p_tmp;
    }
    return nullptr;
  }
private:
  std::unique_ptr<NumericalExpression> p_term_;
};

DECLARE_BINARY_OPERATION_BOOL(IsL, <);
DECLARE_BINARY_OPERATION_BOOL(IsLE, <=);
DECLARE_BINARY_OPERATION_BOOL(IsG, >);
DECLARE_BINARY_OPERATION_BOOL(IsGE, >=);
DECLARE_BINARY_OPERATION_BOOL(IsEQ, ==);
DECLARE_BINARY_OPERATION_BOOL(IsNEQ, !=);

DECLARE_BINARY_OPERATION(Shl, <<);
DECLARE_BINARY_OPERATION(Sar, >>);
DECLARE_PREFIX_UNARY_OPERATION(Inc, +);
DECLARE_PREFIX_UNARY_OPERATION(Dec, -);

////////////////////////////////////////////////////////////////////////
// Stack Variable
////////////////////////////////////////////////////////////////////////

StackVariable::StackVariable(int v)
  : p_exp_(new ImmediateExpression(v)) {}

StackVariable::StackVariable(int v, const std::string& x)
  : p_exp_(new StringExpression(v, x)) {}

StackVariable::StackVariable(Exec* p_exec, int offset)
  : p_exp_(new VariableExpression(p_exec, offset)) {}

StackVariable::StackVariable(const StackVariable& r)
  : p_exp_(r.p_exp_->Clone()) {}

StackVariable::StackVariable(StackVariable&& r) noexcept
  : p_exp_(std::move(r.p_exp_)) {}

const StackVariable& StackVariable::operator=(const StackVariable& rhs) {
  p_exp_.reset(rhs.p_exp_->Clone());
  return (*this);
}

StackVariable::operator int() const { return p_exp_->Evaluate(); }

void StackVariable::swap(StackVariable& x) noexcept {
  std::swap(this->p_exp_, x.p_exp_);
}

void StackVariable::swap(StackVariable&& x) noexcept {
  std::swap(this->p_exp_, x.p_exp_);
}

std::string StackVariable::ToString() const {
  return p_exp_->ToString();
}

#define DECLARE_STACK_ITEM_UNARY_OPERATION(name) \
  void StackVariable::name() {\
    std::unique_ptr<NumericalExpression> p_new_exp(new name##Expression(std::move(p_exp_)));\
    p_exp_ = std::move(p_new_exp);\
  }

#define DECLARE_STACK_ITEM_BINARY_OPERATION(name) \
  void StackVariable::name(StackVariable&& var) {\
    std::unique_ptr<NumericalExpression> p_new_exp(new name##Expression(std::move(p_exp_),\
                                                                        std::move(var.p_exp_)));\
    p_exp_ = std::move(p_new_exp);\
  }

#define DECLARE_STACK_ITEM_SHIFT_OPERATION(name) \
  void StackVariable::name(StackVariable&& var) {\
    std::unique_ptr<NumericalExpression> p_new_exp(new name##Expression(std::move(p_exp_),\
                                                                        std::move(var.p_exp_)));\
    p_exp_ = std::move(p_new_exp);\
  }

DECLARE_STACK_ITEM_UNARY_OPERATION(Neg)
DECLARE_STACK_ITEM_BINARY_OPERATION(Add)
DECLARE_STACK_ITEM_BINARY_OPERATION(Sub)
DECLARE_STACK_ITEM_BINARY_OPERATION(Mul)
DECLARE_STACK_ITEM_BINARY_OPERATION(Div)
DECLARE_STACK_ITEM_BINARY_OPERATION(Mod)
DECLARE_STACK_ITEM_BINARY_OPERATION(And)
DECLARE_STACK_ITEM_BINARY_OPERATION(Or)
DECLARE_STACK_ITEM_BINARY_OPERATION(Xor)
DECLARE_STACK_ITEM_UNARY_OPERATION(Not)
DECLARE_STACK_ITEM_UNARY_OPERATION(Bool)
DECLARE_STACK_ITEM_BINARY_OPERATION(BoolAnd)
DECLARE_STACK_ITEM_BINARY_OPERATION(BoolOr)
DECLARE_STACK_ITEM_UNARY_OPERATION(BoolNot)
DECLARE_STACK_ITEM_BINARY_OPERATION(IsL)
DECLARE_STACK_ITEM_BINARY_OPERATION(IsLE)
DECLARE_STACK_ITEM_BINARY_OPERATION(IsG)
DECLARE_STACK_ITEM_BINARY_OPERATION(IsGE)
DECLARE_STACK_ITEM_BINARY_OPERATION(IsEQ)
DECLARE_STACK_ITEM_BINARY_OPERATION(IsNEQ)
DECLARE_STACK_ITEM_BINARY_OPERATION(Shl)
DECLARE_STACK_ITEM_BINARY_OPERATION(Sar)
DECLARE_STACK_ITEM_UNARY_OPERATION(Inc)
DECLARE_STACK_ITEM_UNARY_OPERATION(Dec)

void StackVariable::OptimizeExpression() {
  NumericalExpression* p_opt = p_exp_->Optimize();
  if (p_opt) {
    p_exp_.reset(p_opt);
  }
}

////////////////////////////////////////////////////////////////////////
// VM Stack
////////////////////////////////////////////////////////////////////////

void VMStack::push(const StackVariable& x) {
  push_back(x);
}

void VMStack::push(StackVariable&& x) {
  push_back(std::move(x));
}

void VMStack::push(int x) {
  push_back(StackVariable(x));
}

void VMStack::push(int index, const std::u16string& name) {
  push_back(StackVariable(index, UTF16ToUTF8(name)));
}

void VMStack::pop() {
  pop_back();
}

StackVariable& VMStack::top() {
  static StackVariable dummy;
  if (empty()) {
    // throw std::out_of_range("VM stack is empty.");
    new(&dummy) StackVariable(0);
    return dummy;
  }
  return back();
}

const StackVariable& VMStack::top() const {
  static StackVariable dummy;
  if (empty()) {
    // throw std::out_of_range("VM stack is empty.");
    new(&dummy) StackVariable(0);
    return dummy;
  }
  return back();
}

const StackVariable& VMStack::peek(int depth) const {
  auto it = rbegin() + depth;
  if (it == rend()) {
    throw std::invalid_argument("VMStack::peek() : too large depth.");
  }
  return *it; // may cause an exception
}

void VMStack::swap(StackVariable& x) noexcept {
  back().swap(x);
}

void VMStack::swap(StackVariable&& x) noexcept {
  back() = std::move(x);
}

void VMStack::swap(int& x) noexcept {
  StackVariable tmp(x);
  x = static_cast<int>(back());
  back() = std::move(tmp);
}

bool VMStack::empty() const {
  return std::vector<StackVariable>::empty();
}

size_t VMStack::size() const {
  return std::vector<StackVariable>::size();
}

////////////////////////////////////////////////////////////////////////
// VM Parser
////////////////////////////////////////////////////////////////////////

VMParser::VMParser(Exec* p)
  : vmdata_(p->GetVMData()), p_exec_(p) {
}

VMParser::~VMParser() {}

int VMParser::ParseOnce(const unsigned char *p_code, std::ostream* p_disasm_strm) {
  int code_size = 1;
  StackVariable temp, temp2;
  std::string str_temp;
  static NullBuffer null_buff;
  static std::ostream null_strm(&null_buff);
  if (p_disasm_strm == nullptr) {
    p_disasm_strm = &null_strm;
  }
  p_vmcode_next_ = nullptr;
  last_exp_.clear();
  switch (*p_code++) {
  case 0x00: // vJmp len:4
    parma_ = *reinterpret_cast<const uint32_t*>(p_code);
    p_vmcode_next_ = p_exec_->GetVMCode() + parma_;
#if 0
    std::cout << "Jump to 0x" << std::hex << parma_
              << std::dec << std::endl;
#endif
    code_size += 4;
    *p_disasm_strm << "jmp   0x" << std::hex << parma_;
    break;
  case 0x01: // vJnz len:4
    parma_ = *reinterpret_cast<const uint32_t*>(p_code);
    if (vm_stack_.top() != 0) {
      p_vmcode_next_ = p_exec_->GetVMCode() + parma_;
#if 0
      std::cout << "Jump to 0x" << std::hex << parma_
                << std::dec << std::endl;
#endif
    }
    code_size += 4;
    *p_disasm_strm << "jnz   0x" << std::hex << parma_;
    break;
  case 0x02: // vJz len:4
    parma_ = *reinterpret_cast<const uint32_t*>(p_code);
    if (vm_stack_.top() == 0) {
      p_vmcode_next_ = p_exec_->GetVMCode() + parma_;
#if 0
      std::cout << "Jump to 0x" << std::hex << parma_
                << std::dec << std::endl;
#endif
    }
    code_size += 4;
    *p_disasm_strm << "jz    0x" << std::hex << parma_;
    break;
  case 0x03: // GetProcAddress len:4+1=5
    parma_ = *reinterpret_cast<const uint32_t*>(p_code);
    temp = StackVariable(static_cast<int>(*(p_code + 4)));
    code_size += 5;
    *p_disasm_strm << "call  " << UTF16ToUTF8(p_exec_->GetExternalFuncName(parma_))
                   << ' ' << std::dec << parma_;
    vm_stack_.push(0);  // return value
    break;
  case 0x04: // GetProcAddress len:1+1=2
    parma_ = static_cast<uint32_t>(*p_code);
    temp = StackVariable(*(p_code + 1));
    code_size += 2;
    *p_disasm_strm << "call  " << UTF16ToUTF8(p_exec_->GetExternalFuncName(parma_))
                   << ' ' << std::dec << parma_;
    vm_stack_.push(0);  // return value
    break;
  case 0x05: // set the LSB of eip to 0
    *p_disasm_strm << "mask  vEip";
    break;
  case 0x06: // vPush R32 len:0
    *p_disasm_strm << "push  R32";
    last_variable_ = vm_stack_.top();
    vm_stack_.swap(StackVariable(p_exec_, last_variable_));
    last_exp_ = vm_stack_.top().ToString();
    break;
  case 0x07: // vPop R32 len:0
    *p_disasm_strm << "pop   R32";
    last_variable_ = vm_stack_.top();
    vm_stack_.pop();
    temp = vm_stack_.top();
    p_exec_->SetVariableValue(last_variable_, temp);
    str_temp = p_exec_->GetVariableName(last_variable_);
    last_exp_.assign(str_temp).append(" = ").append(temp.ToString());
    break;
  case 0x08: // vPush len:4
  case 0x0D: // vPush len:4
    parma_ = *reinterpret_cast<const uint32_t*>(p_code);
    vm_stack_.push(parma_);
    code_size += 4;
    *p_disasm_strm << "push  0x" << std::hex << parma_;
    break;
  case 0x09: // vPushStr len:1
    parma_ = static_cast<uint32_t>(*p_code);
    prev_last_string_ = last_string_;
    GetStringFromVMData(vmdata_, parma_, &last_string_);
    vm_stack_.push(parma_, last_string_);
    ++code_size;
    *p_disasm_strm << "push  \"" << UTF16ToUTF8(last_string_) << '"';
    break;
  case 0x0A: // vPushStr len:2
    parma_ = *reinterpret_cast<const uint16_t*>(p_code);
    prev_last_string_ = last_string_;
    GetStringFromVMData(vmdata_, parma_, &last_string_);
    vm_stack_.push(parma_, last_string_);
    code_size += 2;
    *p_disasm_strm << "push  \"" << UTF16ToUTF8(last_string_) << '"';
    break;
  case 0x0B: // none len:0
    break;
  case 0x0C: // vPushStr len:4
    parma_ = *reinterpret_cast<const uint32_t*>(p_code);
    prev_last_string_ = last_string_;
    GetStringFromVMData(vmdata_, parma_, &last_string_);
    vm_stack_.push(parma_, last_string_);
    code_size += 4;
    *p_disasm_strm << "push  \"" << UTF16ToUTF8(last_string_) << '"';
    break;
  case 0x0E: // vPop len:0
    while (vm_stack_.empty() == false) {
      vm_stack_.pop();
    }
    *p_disasm_strm << "pop";
    break;
  case 0x0F: // vPush 0 len:0
    vm_stack_.push(0);
    *p_disasm_strm << "push  0";
    break;
  case 0x10: // none len:0
    break;
  case 0x11: // vPush len:1
    parma_ = static_cast<uint32_t>(*p_code);
    vm_stack_.push(parma_);
    ++code_size;
    *p_disasm_strm << "push  0x" << std::hex << parma_;
    break;
  case 0x12: // vPush [sp] len:0
    *p_disasm_strm << "push  [sp]";
    break;
  case 0x13: // vNeg len:0
    vm_stack_.top().Neg();
    last_exp_ = vm_stack_.top().ToString();
    *p_disasm_strm << "neg";
    break;
  case 0x14: // vAdd len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().Add(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "add";
    break;
  case 0x15: // vSub len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().Sub(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "sub";
    break;
  case 0x16: // vMul len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().Mul(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "mul";
    break;
  case 0x17: // vDiv len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().Div(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "div";
    break;
  case 0x18: // vMod len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().Mod(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "mod";
    break;
  case 0x19: // vAnd len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().And(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "and";
    break;
  case 0x1A: // vOr len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().Or(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "or";
    break;
  case 0x1B: // vXor len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().Xor(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "xor";
    break;
  case 0x1C: // vNot len:0
    vm_stack_.top().Not();
    last_exp_ = vm_stack_.top().ToString();
    *p_disasm_strm << "not";
    break;
  case 0x1D: // vBOOL(param) len:0
    vm_stack_.top().Bool();
    last_exp_ = vm_stack_.top().ToString();
    *p_disasm_strm << "BOOL(param)";
    break;
  case 0x1E: // vBOOL(param1&&param2) len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().BoolAnd(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "BOOL(param1&&param2)";
    break;
  case 0x1F: // vBOOL(param1||param2) len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().BoolOr(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "BOOL(param1||param2)";
    break;
  case 0x20: // !vBOOL(param) len:0
    vm_stack_.top().BoolNot();
    last_exp_ = vm_stack_.top().ToString();
    *p_disasm_strm << "!BOOL(param)";
    break;
  case 0x21: // vIsL len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().IsL(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "IsL";
    break;
  case 0x22: // vIsLE len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().IsLE(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "IsLE";
    break;
  case 0x23: // vIsNLE(vIsG?) len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().IsG(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "IsNLE";
    break;
  case 0x24: // vIsNL(vIsGE?) len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().IsGE(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "IsNL";
    break;
  case 0x25: // vIsEQ len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().IsEQ(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "IsEQ";
    break;
  case 0x26: // vIsNEQ len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().IsNEQ(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "IsNEQ";
    break;
  case 0x27: // vShl len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().Shl(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "shl";
    break;
  case 0x28: // vSar len:0
    temp = vm_stack_.top();
    vm_stack_.pop();
    vm_stack_.top().Sar(std::move(temp));
    last_exp_ = vm_stack_.top();
    *p_disasm_strm << "sar";
    break;
  case 0x29: // vInc len:0
    vm_stack_.top().Inc();
    last_exp_ = vm_stack_.top().ToString();
    *p_disasm_strm << "inc";
    break;
  case 0x2A: // vDec len:0
    vm_stack_.top().Dec();
    last_exp_ = vm_stack_.top().ToString();
    *p_disasm_strm << "dec";
    break;
  case 0x2B: // vAddReg len:0
    *p_disasm_strm << "AddReg";
    break;
  case 0x2C: // Debug len:0
    *p_disasm_strm << "Debug";
    break;
  case 0x2D: // vCall len:4
    parma_ = *reinterpret_cast<const uint32_t*>(p_code);
    // TODO: StackVariable into Integer
    temp = StackVariable(p_exec_->GetInternalFuncOffset(parma_));
    p_vmcode_next_ = p_exec_->GetVMCode() + temp;
#if 0
      std::cout << "Call 0x" << std::hex << temp
                << std::dec << std::endl;
#endif
    code_size += 4;
    *p_disasm_strm << "call  " << UTF16ToUTF8(p_exec_->GetInternalFuncName(parma_))
                   << "(0x" << std::hex << std::uppercase << temp
                   << std::dec << std::nouppercase << ')';
    return_addrs_.push((p_code - 1 + code_size) - p_exec_->GetVMCode());
    vm_stack_.push(0); // return value
    break;
  case 0x2E: // vAdd len:0 (Address?)
    *p_disasm_strm << "add";
    break;
  case 0x2F: // vFPCopy len:0
    *p_disasm_strm << "FPCopy";
    break;
  case 0x30: // vFPGet len:0
    *p_disasm_strm << "FPGet";
    break;
  case 0x31: // vPush N len:4
    parma_ = *reinterpret_cast<const uint32_t*>(p_code);
    code_size += 4;
    *p_disasm_strm << "initStack " << std::dec << parma_;
    break;
  case 0x32: // vJmp len:1
    parma_ = static_cast<uint32_t>(*p_code);
    p_vmcode_next_ = p_exec_->GetVMCode() + parma_;
#if 0
      std::cout << "Jump to 0x" << std::hex << parma_
                << std::dec << std::endl;
#endif
    ++code_size;
    *p_disasm_strm << "jmp   short 0x" << std::hex << parma_;
    break;
  case 0x33: // vRet len:1
    parma_ = static_cast<uint32_t>(*p_code);
    vm_stack_.swap(parma_);
    ++code_size;
    *p_disasm_strm << "ret   " << std::hex << parma_;
    if (return_addrs_.empty()) {
      return code_size; // end of maliescenario
    }
    if (return_addrs_.empty() == false) {
      p_vmcode_next_ = p_exec_->GetVMCode() + return_addrs_.top();
      return_addrs_.pop();
    }
    break;
  default:
    fprintf(stderr, "Unknown opcode %X\n",*(p_code-1));
    *p_disasm_strm << "unk(0x" << std::hex << static_cast<int>(*(p_code-1)) << ")";
    break;
  }
  if (last_exp_.empty() == false) {
    *p_disasm_strm << "   ; " << last_exp_;
  }
  if (p_vmcode_next_ == nullptr) {
    p_vmcode_next_ = (p_code - 1) + code_size;
  }
  return code_size;
}

bool VMParser::ParseTag(const std::u16string& tag, std::string* tag_name,
                        std::map<std::string, std::string>* attrs) {
  if (tag_name == nullptr || attrs == nullptr) return false;
  std::u16string::const_iterator it = tag.begin();
  for (; it != tag.end(); ++it) {
    const auto c = *it;
    if (c == u'<') { ++it; break; }
    if (isspace(c)) continue;
    return false;
  }
  if (it == tag.end()) return false;
  tag_name->clear();
  attrs->clear();
  for (; it != tag.end(); ++it) {
    const auto c = *it;
    if (isspace(c)) { ++it; break; }
    if (c == '>') return true;
    tag_name->append(1, c);
    continue;
  }
  if (it == tag.end()) return false;
  do {
    std::string attr_key;
    for (; it != tag.end(); ++it) {
      const auto c = *it;
      if (c == u'>') break;
      if (c == u'=') break;
      if (isspace(c)) {
        if (attr_key.empty() == false) {
          ++it;
          break;
        }
      } else {
        attr_key.append(1, c);
      }
    }
    for (; it != tag.end(); ++it) {
      const auto c = *it;
      if (c == u'>') break;
      if (c == u'=') { ++it; break; }
      if (isspace(c) == 0) break;
    }
    std::u16string attr_val;
    int in_quote = '\0';
    for (; it != tag.end(); ++it) {
      const auto c = *it;
      if (in_quote == '\0') {
        if (c == u'>') break;
        if (isspace(c)) continue;
        if (c == u'\'' || c == u'"') {
          in_quote = c;
          continue;
        }
        attr_val.append(1, c);
      } else {
        if (c == in_quote) { ++it; break; }
        attr_val.append(1, c);
      }
    }
    if (attr_key.empty() == false) {
      attrs->insert(std::make_pair(attr_key, UTF16ToUTF8(attr_val)));
    }
  } while (it != tag.end() && *it != u'>');
  return true;
}

void VMParser::ParseCommand(const std::u16string& name) {
  // std::cout << UTF16ToUTF8(name) << std::endl;
  if (name == u"FrameLayer_SendMessage") {
    auto code = vm_stack_.peek(2);
    switch (code & 0x7fffffff) {
    case 0x64+0x0f:  // start of select mode
      last_options_.clear();
      break;
    case 0x64+0x10:  // select item
      {
        const StackVariable& option = vm_stack_.peek(3);
        auto index = option.operator int() & 0xffff;
        auto cond = option.ToString();
        if (cond.empty() == false) {
          cond = cond.substr(0, cond.find(" << 16"));
          if (*cond.rbegin() == ')') cond.pop_back();
          cond = cond.substr(cond.rfind("(")+1);
        }
        last_options_.insert(std::make_pair(index, Options(last_string_, cond)));
      }
      break;
    case 0x64+0x11:  // end of select mode
      break;
#if 0
    default:
      std::cout << "FrameLayer_SendMessage: code 0x" << std::hex << (code & 0x7fffffff)
                << std::dec << std::endl;
#endif
    }
    return;
  }
  if (name == u"MALIE_TAG_BEGIN") {
    // std::cout << "tag begin: " << UTF16ToUTF8(last_string_) << std::endl;
    tag_name = UTF16ToUTF8(last_string_);
    attrs.clear();
    return;
  }
  if (name == u"MALIE_TAG_PARAM") {
    // std::cout << "tag param: " << UTF16ToUTF8(last_string_)
    //          << ", " << UTF16ToUTF8(prev_last_string_) << std::endl;
    attrs.insert(std::make_pair(UTF16ToUTF8(last_string_), UTF16ToUTF8(prev_last_string_)));
    return;
  }
  if (name == u"MALIE_TAG_END") {
    // std::cout << "tag end" << std::endl;
    export_to_->EvaluateTag(tag_name, attrs);
    return;
  }
  if (name == u"MALIE_FONTPOSITION") {
#if 0
    int pos = vm_stack_.top() & 0x7fffffff;
    std::cout << "&MALIE_FONTPOSITION(" << pos
              << ") in " << export_to.GetHigherLabel() << std::endl;
#endif
    // export_to_->ChangePosition(pos);
    return;
  }
  if (name == u"MALIE_FONTSIZE") {
    int size = vm_stack_.top() & 0x7fffffff;
#if 0
    std::cout << "&MALIE_FONTSIZE(" << size
              << ") in " << export_to.GetHigherLabel() << std::endl;
#endif
    export_to_->ChangeSize(size);
    return;
  }
  if (name == u"MALIE_FONTCOLOR") {
    int color = vm_stack_.top() & 0x7fffffff;
    export_to_->ChangeColor(color);
    return;
  }
}

size_t VMParser::ParseJmpTable(std::vector<int>& jmp_table) {
  const unsigned char* p_jmp_table_start = p_vmcode_curr_;
  const decltype(parma_) MALIE_END = p_exec_->GetInternalFuncId(u"MALIE_END");
  for (; *p_vmcode_curr_ != 0x33; ) {
    int size_code = ParseOnce(p_vmcode_curr_);
    if (*p_vmcode_curr_ == 0x2D) {
      if (parma_ == MALIE_END) {
        break;
      }
    } else if (*p_vmcode_curr_ == 0) {
      jmp_table.push_back(parma_);
    }
    p_vmcode_curr_ += size_code;
  }
  return p_vmcode_curr_ - p_jmp_table_start;
}

int VMParser::SearchNextLabel(int option, std::u16string* p_label) {
  // assert(*(p_vmcode_curr_ - 1) == 0x07);
  auto result_offset = p_exec_->GetVariableOffset("result");
  if (result_offset == -1) {
    return -1;
  }
  p_exec_->SetVariableValue(result_offset, option);
  const auto p_base = p_exec_->GetVMCode();
  auto p = p_vmcode_curr_;
  int offset = p - p_base;
  int prev_offset;
  // std::u16string label_name;
  const decltype(parma_) MALIE_LABLE = p_exec_->GetInternalFuncId(u"MALIE_LABLE");
  parma_ = 0;
  while (*p != 0x33) {
    prev_offset = offset;
    ParseOnce(p);
    p = p_vmcode_next_;
    offset = p - p_base;
    if (parma_ == MALIE_LABLE) {
      if (p_label != nullptr) {
        p_label->assign(last_string_);
      }
      return prev_offset;
    }
    if (option == -1 && *p == 0x02) { // vJz
      --p;
      --offset;
      for (; p > p_base; --p, --offset) {
        if (p_exec_->IsTempLabelOffset(offset)) {
          if (p_label != nullptr) {
            p_label->assign(p_exec_->GetTempLabelName(offset));
          }
          next_tmp_lbl_ofs_ = offset;
          return offset;
        }
      }
      return -1;
    }
    // std::cout << "Offset: 0x" << std::hex << offset << std::endl;
  }
  return -1;
}

void VMParser::ParseScenario(ExecTextEvaluator* p_eval) {
  export_to_ = p_eval;
  int offset = p_exec_->GetInternalFuncOffset(std::u16string(u"maliescenario"));
  p_vmcode_curr_ = p_exec_->GetVMCode() + offset;
  next_tmp_lbl_ofs_ = -1;
  const decltype(parma_) _ms_message = p_exec_->GetInternalFuncId(u"_ms_message");
  const decltype(parma_) MALIE_NAME = p_exec_->GetInternalFuncId(u"MALIE_NAME");
  const decltype(parma_) MALIE_LABLE = p_exec_->GetInternalFuncId(u"MALIE_LABLE");
  const decltype(parma_) tag = p_exec_->GetExternalFuncId(u"tag");
  bool has_loaded_msg = false;
  std::vector<int> jmp_table;
  std::vector<int>::iterator it;
  for (; *p_vmcode_curr_ != 0x33/*vRet*/; ) {
#if 0
    std::cout << "VMCode offset: 0x" << std::hex << std::uppercase
              << (p_vmcode_curr_ - p_exec_->GetVMCode()) << std::dec << std::nouppercase
              << std::endl;
#endif
    int code_size = ParseOnce(p_vmcode_curr_);
    // std::cout << "Code size: " << code_size << std::endl;
    if (jmp_table.size()) {
      if (it != jmp_table.end() && offset > *it) {
        ++it;
      }
    }
    if (next_tmp_lbl_ofs_ == offset) {
      p_eval->EvaluateLabel(p_exec_->GetTempLabelName(next_tmp_lbl_ofs_));
      next_tmp_lbl_ofs_ = -1;
    }
    if (*p_vmcode_curr_ == 0x2D || *p_vmcode_curr_ == 0x04 ||
        *p_vmcode_curr_ == 0x03) { // vCall
      if (parma_ == tag) {
        ParseTag(last_string_, &tag_name, &attrs);
        export_to_->EvaluateTag(tag_name, attrs);
      } else if (parma_ == _ms_message) {
        vm_stack_.pop();
        // while (vm_stack_.empty() == false) vm_stack_.pop();
        std::auto_ptr<ExecTextExpression> exp(p_exec_->ParseText(vm_stack_.top() & 0x7fffffff));
        if (exp.get() != nullptr) {
          export_to_->Evaluate(exp.get());
        }
        has_loaded_msg = true;
      } else if (parma_ == MALIE_NAME) {
        vm_stack_.pop();
        // std::cout << UTF16ToUTF8(last_string_) << std::endl;
        export_to_->EvaluateCharaName(last_string_);
      } else if (parma_== MALIE_LABLE) {
        if (has_loaded_msg == false && last_string_ == u"_link") {
          code_size += ParseJmpTable(jmp_table);
          it = jmp_table.begin();
          offset += code_size;
          last_string_.clear();
          continue;
        }
        export_to_->EvaluateLabel(last_string_);
      } else {  // other command
        switch (*p_vmcode_curr_) {
        case 0x03:
        case 0x04:
          ParseCommand(p_exec_->GetExternalFuncName(parma_));
        case 0x2D:
          ParseCommand(p_exec_->GetInternalFuncName(parma_));
        }
      }
      last_string_.clear();
      while (return_addrs_.empty() == false) return_addrs_.pop();
    } else if (*p_vmcode_curr_ == 0x07) { // vPop R32
      if (last_exp_.find("result = ") != std::string::npos) {
        export_to_->StartSelectMode();
        for (auto& opt : last_options_) {
          opt.second.offset = SearchNextLabel(opt.first, &opt.second.label);
          export_to_->AddOption(UTF16ToUTF8(opt.second.name),
                                UTF16ToUTF8(opt.second.label),
                                opt.second.cond);
        }
        export_to_->EndSelectMode();
        int ofst = 0x7fffffff;
        for (const auto& opt : last_options_) {
          if (opt.second.offset < ofst && opt.second.offset >= 0) {
            ofst = opt.second.offset;
            last_string_ = opt.second.label;
          }
        }
        if (ofst != 0x7fffffff) {
          p_vmcode_curr_ = p_exec_->GetVMCode() + ofst;
          offset = ofst;
          continue;
        }
      } else {
        p_eval->SetParameter(last_exp_);
      }
    } else if (*p_vmcode_curr_ == 0x00) { // vJmp
      if (p_exec_->IsTempLabelOffset(offset) == false) {
        std::u16string next_label;
        int ofst = SearchNextLabel(-1, &next_label);
        if (ofst != -1) {
          p_eval->Jump(UTF16ToUTF8(next_label), std::string());
        }
      }
    } else if (*p_vmcode_curr_ == 0x02) { // vJz
      StackVariable var(0);
      vm_stack_.swap(var);
      std::string var_str = var.ToString();
      var.BoolNot();
      var.OptimizeExpression();
      var_str = var.ToString();
      if (p_exec_->IsTempLabelOffset(offset) == false) {
        std::u16string next_label;
        int ofst = SearchNextLabel(-1, &next_label);
        if (ofst != -1) {
          if (var_str == "1") var_str.clear();
          p_eval->Jump(UTF16ToUTF8(next_label), var_str);
        }
      }
    }
    p_vmcode_curr_ += code_size;
    offset += code_size;
  }
}

void VMParser::Disassemble(const std::string& file_name) {
  std::ofstream f(file_name);
  if (f.is_open() == false) return;
  const unsigned char* p_vmcode = p_exec_->GetVMCode();
  const size_t length = p_exec_->GetVMCodeSize();
  size_t i;
  try {
    for (i = 0; i < length; ) {
      f << std::setw(8) << std::setfill('0') << std::hex << std::uppercase
        << i << ":   " << std::setfill(' ') << std::dec;
      i += ParseOnce(p_vmcode + i, &f);
      f << "  [stack_size: " << vm_stack_.size() << "]";
      f << std::endl;
    }
  } catch (std::out_of_range& e) {
    std::cerr << e.what() << " (0x" << std::setw(8) << std::setfill('0')
              << std::hex << i << ")." << std::endl;
  }
  f.flush();
  f.close();
}

void VMParser::SimulateRoutes() {
  const int start_offset = p_exec_->GetInternalFuncOffset(u"maliescenario");
  p_vmcode_curr_ = p_exec_->GetVMCode() + start_offset;
  while (vm_stack_.empty() == false) vm_stack_.pop();
  while (return_addrs_.empty() == false) return_addrs_.pop();
  p_exec_->ResetLocalVariableValues();
  std::string tag_name;
  std::map<std::string, std::string> attrs;
  std::map<int, std::string> options;
  int option_num = 0;
  std::string map_name;
  int map_rc_offset = -1;
#if 0
  std::ofstream sim_disasm("sim_disasm.txt");
  std::ostringstream disasm_line;
  do {
    disasm_line << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
                << (p_vmcode_curr_ - p_exec_->GetVMCode())
                << std::dec << std::nouppercase << ":   ";
    ParseOnce(p_vmcode_curr_, &disasm_line);
    if (last_exp_.empty() == false) {
      sim_disasm << disasm_line.str() << std::endl;
    }
    disasm_line.str("");
#else
  do {
    ParseOnce(p_vmcode_curr_);
#endif
    if (p_vmcode_next_ == nullptr ||
        (*p_vmcode_curr_ == 0x2D && p_exec_->GetInternalFuncName(parma_) == u"MALIE_END")) {
      char c;
      std::cout << "Continue? (Y/n) : ";
      std::cin >> c;
      if (c == 'N' || c == 'n') {
        break;
      }
      p_vmcode_curr_ = p_exec_->GetVMCode() + start_offset;
      continue;
    }
    if (*p_vmcode_curr_ == 0x04 || *p_vmcode_curr_ == 0x03) { // vCall
      const auto func_name = UTF16ToUTF8(p_exec_->GetExternalFuncName(parma_));
      if (func_name == "tag") {
        ParseTag(last_string_, &tag_name, &attrs);
        // std::cout << tag_name << std::endl;
        if (tag_name == "map") {
          map_name = attrs["src"];
          std::string result = attrs["result"];
          map_rc_offset = p_exec_->GetVariableOffset(result);
          options.clear();
        } else if (tag_name == "mapitem") {
          std::string str_id = attrs["id"];
          std::string str_src = attrs["src"];
          options.insert(make_pair(std::atoi(str_id.c_str()), str_src));
        } else if (tag_name == "mapwait") {
          std::cout << '[';
          for (const auto& opt : options) {
            std::cout << opt.first << ":'" << opt.second << "' ";
          }
          std::cout << "\b] ? ";
          std::cin >> option_num;
          if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore();
          }
          std::cout << "selected '" << options[option_num]
                    << "' in map '" << map_name << "'" << std::endl;
          p_exec_->SetVariableValue(map_rc_offset, option_num);
        } else if (tag_name == "chapter") {
          std::cout << "■ [CHAPTER] " << attrs["name"] << std::endl;
        }
      } else if (func_name == "FrameLayer_SendMessage") {
        auto code = vm_stack_.peek(2);
        switch (code) {
        case 0x64+0x0f:  // start of select mode
          options.clear();
          break;
        case 0x64+0x10:  // select item
          option_num = static_cast<int>(vm_stack_.peek(3));
          if (option_num & 0x10000) {
            options.insert(make_pair(option_num & 0xffff, UTF16ToUTF8(last_string_)));
          }
          break;
        case 0x64+0x11:  // end of select mode
          if (options.empty()) {
            std::cout << "There are no selectable options." << std::endl;
          } else {
            std::cout << '[';
            for (const auto& opt : options) {
              std::cout << opt.first << ":'" << opt.second << "' ";
            }
            std::cout << "\b] ? ";
            std::cin >> option_num;
            if (std::cin.fail()) {
              std::cin.clear();
              std::cin.ignore();
            }
          }
          break;
        }
      } else if (func_name == "System_GetResult") {
        vm_stack_.top() = option_num;
        std::cout << "selected '" << options[option_num] << "'" << std::endl;
      } else if (func_name == "qj_flag") {
        vm_stack_.pop();
      }
    }
#if 0
    if (*p_vmcode_curr_ == 0x06) { // vPush R32

      const auto var_name = p_exec_->GetVariableName(last_variable_);
      if (var_name == "result" || var_name == "rc") {
        int val = p_exec_->GetVariableValue(last_variable_);
        std::cout << var_name << " is " << val << std::endl;
      }

    }
#endif
    if (*p_vmcode_curr_ == 0x2D) {
      const auto func_name = UTF16ToUTF8(p_exec_->GetInternalFuncName(parma_));
      if (func_name == "MALIE_LABLE") {
        std::cout << "■ [LABEL] " << UTF16ToUTF8(last_string_) << std::endl;
      }
    }
    if (*p_vmcode_curr_ == 0x07) { // vPop R32
      const auto var_name = p_exec_->GetVariableName(last_variable_);
      if (var_name.empty() == false && var_name != "malie") {
        std::string exp = last_exp_.substr(last_exp_.find("=") + 2);
        int val = p_exec_->GetVariableValue(last_variable_);
        std::cout << var_name << " := " << val;
        if (exp != std::to_string(val)) {
          std::cout << " (" << exp << ')';
        }
        std::cout << std::endl;
        // std::cout << last_exp_ << " ( = " << val << ")" << std::endl;
      }
    }
    p_vmcode_curr_ = p_vmcode_next_;
  } while (true);
}

} // namespace mlib
