#pragma once

/* vmparser.h (updated on 2017/09/08)
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

#include "exec.h"
#include <stack>
#include <vector>
#include <map>
#include <string>

namespace mlib {

////////////////////////////////////////////////////////////////////////
// Numerical Expression
////////////////////////////////////////////////////////////////////////

class NumericalExpression {
public:
  virtual ~NumericalExpression() = default;
  virtual bool IsBoolean() const { return false; }
  virtual bool IsPolynomial() const = 0;
  virtual int Evaluate() const = 0;
  virtual std::string ToString() const = 0;
  virtual NumericalExpression* Clone() const = 0;
  virtual NumericalExpression* Optimize() { return nullptr; }
protected:
  std::string& PutInParenthesesIfNeeded(std::string* s) const;
  static std::string MonomialToString(const char* oper,
                                      const std::unique_ptr<NumericalExpression>& p_term);
  static std::string PolynomialToString(const std::string& oper,
                                        const std::unique_ptr<NumericalExpression>& p_term1,
                                        const std::unique_ptr<NumericalExpression>& p_term2);
};

////////////////////////////////////////////////////////////////////////
// Stack Variable
////////////////////////////////////////////////////////////////////////

class StackVariable {
public:
  StackVariable(int v = 0);
  StackVariable(int v, const std::string& x);
  StackVariable(Exec* p_exec, int offset);
  StackVariable(const StackVariable&);
  StackVariable(StackVariable&&) noexcept;
  const StackVariable& operator=(const StackVariable& rhs);
  operator int() const;
  void swap(StackVariable& x) noexcept;
  void swap(StackVariable&& x) noexcept;
  std::string ToString() const;
  void OptimizeExpression();

  void Neg();
  void Add(StackVariable&&);
  void Sub(StackVariable&&);
  void Mul(StackVariable&&);
  void Div(StackVariable&&);
  void Mod(StackVariable&&);
  void And(StackVariable&&);
  void Or(StackVariable&&);
  void Xor(StackVariable&&);
  void Not();
  void Bool();
  void BoolAnd(StackVariable&&);
  void BoolOr(StackVariable&&);
  void BoolNot();
  void IsL(StackVariable&&);
  void IsLE(StackVariable&&);
  void IsG(StackVariable&&);
  void IsGE(StackVariable&&);
  void IsEQ(StackVariable&&);
  void IsNEQ(StackVariable&&);
  void Shl(StackVariable&&);
  void Sar(StackVariable&&);
  void Inc();
  void Dec();

private:
  std::unique_ptr<NumericalExpression> p_exp_;
};

////////////////////////////////////////////////////////////////////////
// VM Stack
////////////////////////////////////////////////////////////////////////

class VMStack : private std::vector<StackVariable> {
public:
  void push(const StackVariable& x);
  void push(StackVariable&& x);
  void push(int x);
  void push(int index, const std::u16string& name);
  void pop();
  StackVariable& top();
  const StackVariable& top() const;
  const StackVariable& peek(int depth) const;
  void swap(StackVariable& x) noexcept;
  void swap(StackVariable&& x) noexcept;
  void swap(int& x) noexcept;
  bool empty() const;
  size_t size() const;
};


////////////////////////////////////////////////////////////////////////
// VM Code Parser
////////////////////////////////////////////////////////////////////////

class VMParser {
public:
  explicit VMParser(Exec *p);
  ~VMParser();
  int ParseOnce(const unsigned char *p_code, std::ostream* p_disasm_strm = nullptr);
  void ParseScenario(ExecTextEvaluator* p_eval);
  size_t ParseJmpTable(std::vector<int>& jmpTable);
  int SearchNextLabel(int option, std::u16string* p_label);

  void Disassemble(const std::string& file_name);
  void SimulateRoutes();

protected:
  struct Options {
    std::u16string name;
    std::string cond;
    std::u16string label;
    int offset;
    Options(const std::u16string& nm, const std::string& cnd)
      : name(nm), cond(cnd) {}
  };

  bool ParseTag(const std::u16string& tag, std::string* tag_name,
                std::map<std::string, std::string>* attrs);
  void ParseCommand(const std::u16string& name);

private:
  const unsigned char* p_vmcode_curr_;
  const unsigned char* p_vmcode_next_;
  uint32_t parma_;
  StackVariable last_variable_;
  std::u16string last_string_;
  std::u16string prev_last_string_;
  std::string last_exp_;
  // std::stack<StackVariable> vm_stack_;
  VMStack vm_stack_;
  std::stack<uint32_t> return_addrs_;
  const std::vector<char16_t>& vmdata_;
  Exec *p_exec_;

  ExecTextEvaluator* export_to_;
  std::string tag_name;
  std::map<std::string, std::string> attrs;
  std::map<int, Options> last_options_;
  int next_tmp_lbl_ofs_;
};

} // namespace mlib
