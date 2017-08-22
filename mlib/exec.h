#pragma once

/* exec.h (updated on 2017/06/28)
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

#include "mlib.h"
#include <fstream>
#include <stack>

namespace mlib {

////////////////////////////////////////////////////////////////////////
/// \brief ExecTextEvaluator Classes
////////////////////////////////////////////////////////////////////////

class ExecTextExpression;

class ExecTextEvaluator {
public:
  ExecTextEvaluator();
  ~ExecTextEvaluator() = default;
  virtual std::string GetHeader() { return std::string(); }
  virtual std::string GetFooter() { return std::string(); }
  virtual std::string Evaluate(ExecTextExpression *p_exp) = 0;
  virtual std::string EvaluateNonterminal(const std::vector<std::string> &list) = 0;
  virtual std::string EvaluateTerminal(const std::u16string &text) = 0;
  virtual std::string EvaluateColored(const std::string &text,
                              int color_red, int color_green, int color_blue) = 0;
  virtual std::string EvaluateWithFontSize(const std::string &text, int font_size) = 0;
  virtual std::string EvaluateSpecial(int character) = 0;
  virtual std::string EvaluateWaitingForKeyIn() { return std::string(); }
  virtual std::string EvaluateWithRuby(const std::string &text, const std::string &ruby) = 0;
  virtual std::string EvaluateWithVoice(const std::string &text, const std::string &voice_name) = 0;
  virtual std::string EvaluateWithDot(const std::u16string &text, const std::string &dot) = 0;

  void EvaluateTag(const std::string& tag_name, const std::map<std::string, std::string>& attrs);
  void EvaluateLabel(const std::u16string& label);
  void EvaluateCharaName(const std::u16string& name);

  bool IsNovelMode() const { return novel_mode_; }
  const std::string& GetLabel() const { return label_; }
  const std::string& GetHigherLabel() const { return higher_label_; }
  const std::string& GetCharaName() const { return chara_name_; }
  std::string PopCharaName();

  virtual void StartSelectMode() {}
  virtual void AddOption(const std::string& /*option*/, const std::string& /*label*/,
                         const std::string& /*cond*/) {}
  virtual void EndSelectMode() {}
  virtual void SetParameter(const std::string& /*exp*/) {}
  virtual void Jump(const std::string& /*label*/, const std::string& /*cond*/) {}
  virtual void ChangeSize(int /*new_size*/) {}
  virtual void ChangeColor(int /*new_color*/) {}

protected:
  virtual void OnSwitchedNovelMode(bool /*novel_mode*/) {}
  virtual void OnChangedHigherLabel(const std::string& /*new_label*/) {}
  virtual void OnChangedLowerLabel(const std::string& /*new_label*/,
                                   bool /*changed_higher_label*/) {}
  virtual void OnChangedCG(const std::string& /*src*/, const std::string& /*mask*/,
                           const std::string& /*display_time*/, bool /*msg_hide*/) {}
  virtual void OnDisplayedChara(const std::string& /*na*/, const std::string& /*dr*/,
                                const std::string& /*ex*/, const std::string& /*pl*/,
                                const std::string& /*tm*/) {}
  virtual void OnDisplayedChara(const std::string& /*src*/, const std::string& /*position*/,
                                const std::string& /*display_time*/, const std::string& /*opacity*/) {}
  virtual void OnErasedChara(const std::string& /*na*/) {}
  virtual void OnChangeMsgFrame(const std::string& /*src*/, const std::string& /*visibility*/) {}

private:
  bool novel_mode_;
  std::string label_;
  std::string higher_label_;
  std::string chara_name_;
};

class ExecTextToASText : public ExecTextEvaluator {
public:
  ExecTextToASText(const std::string& file_name);
  std::string Evaluate(ExecTextExpression *p_exp);
  std::string EvaluateNonterminal(const std::vector<std::string> &list);
  std::string EvaluateTerminal(const std::u16string &text);
  std::string EvaluateColored(const std::string &text,
                      int color_red, int color_green, int color_blue);
  std::string EvaluateWithFontSize(const std::string &text, int font_size);
  std::string EvaluateSpecial(int character);
  std::string EvaluateWaitingForKeyIn() ;
  std::string EvaluateWithRuby(const std::string &text, const std::string &ruby);
  std::string EvaluateWithVoice(const std::string &text, const std::string &voice_name);
  std::string EvaluateWithDot(const std::u16string &text, const std::string &dot);

  void StartSelectMode();
  void AddOption(const std::string& option, const std::string& label, const std::string& cond);
  void EndSelectMode();
  void SetParameter(const std::string& exp);
  void Jump(const std::string& label, const std::string& cond);

protected:
  void OnSwitchedNovelMode(bool novel_mode);
  void OnChangedLowerLabel(const std::string& new_label, bool changed_higher_label);
  void OnChangedCG(const std::string& src, const std::string& mask,
                   const std::string& display_time, bool msg_hide);
  void OnDisplayedChara(const std::string& na, const std::string& dr,
                        const std::string& ex, const std::string& pl,
                        const std::string& tm);
  void OnDisplayedChara(const std::string& src, const std::string& position,
                        const std::string& display_time, const std::string& opacity);
  void OnErasedChara(const std::string& na);
  void OnChangeMsgFrame(const std::string& src, const std::string& visibility);

private:
  std::ofstream ofs_;
  int option_count_;
};

class ExecTextToXhtml : public ExecTextEvaluator {
public:
  ExecTextToXhtml(const std::string& product, bool vertical = true);
  ~ExecTextToXhtml();
  std::string GetHeader();
  std::string GetFooter();
  std::string Evaluate(ExecTextExpression *p_exp);
  std::string EvaluateNonterminal(const std::vector<std::string> &list);
  std::string EvaluateTerminal(const std::u16string &text);
  std::string EvaluateColored(const std::string &text,
                      int color_red, int color_green, int color_blue);
  std::string EvaluateWithFontSize(const std::string &text, int font_size);
  std::string EvaluateSpecial(int character);
  std::string EvaluateWithRuby(const std::string &text, const std::string &ruby);
  std::string EvaluateWithVoice(const std::string &text, const std::string &voice_name);
  std::string EvaluateWithDot(const std::u16string &text, const std::string &dot);

  void StartSelectMode();
  void AddOption(const std::string& option, const std::string&, const std::string&);
  void EndSelectMode();
  void ChangeSize(int new_size);
  void ChangeColor(int new_color);

protected:
  void OnSwitchedNovelMode(bool novel_mode);
  void OnChangedHigherLabel(const std::string& new_label);
  void OnChangedLowerLabel(const std::string& new_label, bool changed_higher_label);

private:
  std::string product_name_;
  std::string base_dir_;
  std::string uuid_;
  std::map<std::string, std::ofstream*> xhtmls_;
  std::vector<std::string> content_order_;
  std::ofstream* p_ofs_;
  int curr_size_;
  int curr_color_;
};

////////////////////////////////////////////////////////////////////////
/// \brief ExecTextExpression Classes
////////////////////////////////////////////////////////////////////////

class ExecTextExpression {
public:
  virtual ~ExecTextExpression() = default;
  virtual void DotTest() = 0;
  virtual std::string Evaluate(ExecTextEvaluator *) = 0;
};

class ExecTextNonterminal : public ExecTextExpression {
public:
  ExecTextNonterminal() = default;
  ExecTextNonterminal(std::vector<ExecTextExpression *> &&l) noexcept
    : list_(std::move(l)) {}
  ~ExecTextNonterminal() {
    while (list_.empty() == false) {
      ExecTextExpression *exp = list_.back();
      list_.pop_back();
      delete exp;
    }
  }
  std::vector<ExecTextExpression *> TakeOutList() {
    return std::move(list_);
  }
  void DotTest();
  std::string Evaluate(ExecTextEvaluator *eval);
private:
  std::vector<ExecTextExpression *> list_;
};

class ExecTextTerminal : public ExecTextExpression {
public:
  ExecTextTerminal(const std::u16string &text) : text_(text) {}
  const std::u16string &GetText() const { return text_; }
  void DotTest() {}
  std::string Evaluate(ExecTextEvaluator *eval);
private:
  std::u16string text_;
};

class ExecTextColored : public ExecTextExpression {
public:
  ExecTextColored(ExecTextExpression *p_text,
                  int color_red, int color_green, int color_blue)
    : p_text_(p_text),
      color_red_(color_red),
      color_green_(color_green),
      color_blue_(color_blue) {}
  ~ExecTextColored() {
    delete p_text_;
  }
  void DotTest();
  std::string Evaluate(ExecTextEvaluator *eval);
private:
  ExecTextExpression *p_text_;
  int color_red_;
  int color_green_;
  int color_blue_;
};

class ExecTextWithFontSize : public ExecTextExpression {
public:
  ExecTextWithFontSize(ExecTextExpression *p_text, int font_size)
    : p_text_(p_text), font_size_(font_size) {}
  ~ExecTextWithFontSize() {
    delete p_text_;
  }
  void DotTest();
  std::string Evaluate(ExecTextEvaluator *eval);
private:
  ExecTextExpression *p_text_;
  int font_size_;
};

class ExecTextSpecial : public ExecTextExpression {
public:
  ExecTextSpecial(int character) : character_(character) {}
  void DotTest() {}
  std::string Evaluate(ExecTextEvaluator *eval);
private:
  int character_;
};

class ExecTextWithRuby : public ExecTextExpression {
public:
  ExecTextWithRuby(ExecTextExpression *p_text, const std::string &ruby)
    : p_text_(p_text), ruby_(ruby) {}
  ~ExecTextWithRuby() {
    delete p_text_;
  }
  const std::string &GetRuby() const { return ruby_; }
  ExecTextExpression *GetExpression() { return p_text_; }
  void DotTest();
  std::string Evaluate(ExecTextEvaluator *eval);
private:
  ExecTextExpression *p_text_;
  std::string ruby_;
};

class ExecTextWaitingForKeyIn : public ExecTextExpression {
public:
  ExecTextWaitingForKeyIn() = default;
  void DotTest() {}
  std::string Evaluate(ExecTextEvaluator* eval);
};

class ExecTextWithVoice : public ExecTextExpression {
public:
  ExecTextWithVoice(ExecTextExpression *p_text, const std::string &voice)
    : p_text_(p_text), voice_(voice) {}
  ~ExecTextWithVoice() {
    delete p_text_;
  }
  void DotTest();
  std::string Evaluate(ExecTextEvaluator *eval);
private:
  ExecTextExpression *p_text_;
  std::string voice_;
};

class ExecTextWithDot : public ExecTextExpression {
public:
  ExecTextWithDot(const std::u16string &text, const std::string &dot)
    : text_(text), dot_(dot) {}
  ~ExecTextWithDot() {}
  void DotTest() {}   // dummy
  std::string Evaluate(ExecTextEvaluator *eval);
private:
  std::u16string text_;
  std::string dot_;
};

////////////////////////////////////////////////////////////////////////
/// \brief Exec Class
////////////////////////////////////////////////////////////////////////

class Exec {
public:
  Exec(VersionedFile *p_file, const std::string& product);

  int GetVariableOffset(const std::string& name) const;
  std::string GetVariableName(int offset) const;
  int GetVariableValue(int offset) const;
  void SetVariableValue(int offset, int value);
  void ResetLocalVariableValues();

  int GetExternalFuncId(const std::u16string& name) const;
  std::u16string GetExternalFuncName(int id) const;
  int GetExternalFuncOffset(int id) const;
  int GetExternalFuncOffset(const std::u16string& name) const;

  int GetInternalFuncId(const std::u16string& name) const;
  std::u16string GetInternalFuncName(int id) const;
  int GetInternalFuncOffset(int id) const;
  int GetInternalFuncOffset(const std::u16string& name) const;

  int GetLabelOffset(const std::u16string& label_name) const;
  bool IsLabelOffset(int offset) const;
  std::u16string GetLabelName(int offset) const;

  bool IsTempLabelOffset(int offset) const;
  std::u16string GetTempLabelName(int offset) const;


  const std::vector<char16_t>& GetVMData() const { return vmdata_; }
  const unsigned char *GetVMCode() const { return &vmcode_[0]; }
  size_t GetVMCodeSize() const { return vmcode_.size(); }

  std::string ParseText(ExecTextEvaluator *p_eval);
  ExecTextExpression *ParseText(int index);
  void ClearParsedText();

  bool OutputVariableList(const std::string& file_name);
  bool OutputFunctionList(const std::string& file_name);
  bool OutputLabelList(const std::string& file_name);

private:
  // exec1 - variable block
  struct Exec1 {
    std::u16string name;
    enum Type : int {
      kVariable = 0x06,
      kString = 0x10,
      kArray = 0x12,
      kFunction = 0x13
    };
    Type type;
    int size;
    int scope;
    int shift;
    int in_func3;
    int init_value;
    int value;
  };

  // exec2 - function parse block
  struct Exec2 {
    std::u16string func_name;
    int id;
    int in_label_block;
    int code_offset;
  };

  struct FuncInfo {
    std::u16string name;
    int offset;
    FuncInfo(std::u16string nm, int ofs)
      : name(nm), offset(ofs) {}
  };

  bool ReadExec1();
  bool ReadExec2();
  bool ReadExec3();
  bool ReadExec4();
  bool ReadExec5();
  bool ReadExec6();
  bool ReadExec7();
  bool CalculateExec6And7Offset();

  std::vector<Exec1>::iterator FindVariable(int offset);
  std::vector<Exec1>::const_iterator FindVariable(int offset) const;

  ExecTextExpression *ParseText(const std::u16string& text);

  VersionedFile *p_file_;
  const std::string product_;

  // meta
  int exec1_length_;
  int exec2_length_;
  int exec3_length_;
  int exec4_offset_;
  int exec5_offset_;
  int exec6_offset_;
  int exec7_offset_;

  std::vector<Exec1> exec1_;
  int exec1_size_;

  // exec3 - label parse block
  std::vector<Exec2> exec2_;

  // std::vector<Exec3> exec3_;
  std::map<int, std::u16string> exec3_;

  std::map<int, FuncInfo> ext_funcs_;
  std::map<int, FuncInfo> int_funcs_;
  std::map<int, std::u16string> labels_;
  std::map<int, std::u16string> tmp_lbls_;

  // exec4 - vmdata
  std::vector<char16_t> vmdata_;

  // exec5 - vmcode
  std::vector<unsigned char> vmcode_;

  // exec6 - string index table
  std::map<int, int> exec6_;

  // exec7 - strings
  std::vector<std::u16string> exec7_;
  std::vector<ExecTextExpression *> expressions_;
};

} // namespace mlib
