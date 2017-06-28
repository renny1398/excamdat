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

namespace mlib {

////////////////////////////////////////////////////////////////////////
/// \brief ExecTextEvaluator Classes
////////////////////////////////////////////////////////////////////////

class ExecTextExpression;

class ExecTextEvaluator {
public:
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
  virtual std::string EvaluateWithRuby(const std::string &text, const std::string &ruby) = 0;
  virtual std::string EvaluateWithVoice(const std::string &text, const std::string &voice_name) = 0;
  virtual std::string EvaluateWithDot(const std::u16string &text, const std::string &dot) = 0;
};

class ExecTextToASText : public ExecTextEvaluator {
public:
  ExecTextToASText() = default;
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
};

class ExecTextToXhtml : public ExecTextEvaluator {
public:
  ExecTextToXhtml() = default;
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
  Exec(VersionedFile *p_file);

  std::string ParseText(ExecTextEvaluator *p_eval);
  void ClearParsedText();

private:
  bool ReadExec1();
  bool ReadExec2();
  bool ReadExec3();
  bool ReadExec6();
  bool ReadExec7();

  bool CalculateExec4Offset();
  bool CalculateExec5Offset();
  bool CalculateExec6And7Offset();

  ExecTextExpression *ParseText(const std::__1::u16string& text);

  VersionedFile *p_file_;

  // meta
  int exec1_length_;
  int exec2_length_;
  int exec3_length_;
  int exec4_offset_;
  int exec5_offset_;
  int exec6_offset_;
  int exec7_offset_;

  // exec1
  struct Exec1 {
    std::string name;
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
  };
  std::vector<Exec1> exec1_;
  int exec1_size_;

  // exec2
  struct Exec2 {
    std::string func_name;
    int in_func3;
    int func_shift;
  };
  std::vector<Exec2> exec2_;

  // exec3
  struct Exec3 {
    std::string func_name;
    int func5_shift;
  };
  std::vector<Exec3> exec3_;

  // exec6
  std::map<int, int> exec6_;

  // exec7
  std::vector<std::u16string> exec7_;
  std::vector<ExecTextExpression *> expressions_;
};

} // namespace mlib
