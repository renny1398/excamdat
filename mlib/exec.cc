/* exec.cc (updated on 2017/06/28)
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
#include <iostream>
#include <iomanip>
#include <sstream>

namespace {

void ReadString(mlib::VersionedFile *p_file, size_t utf16_length, std::string *dest) {
  char16_t *buf = new char16_t[(utf16_length/2)+1];
  p_file->Read(utf16_length, buf);
  dest->assign(mlib::UTF16ToUTF8(buf, utf16_length/2));
  delete [] buf;
}

} // namespace

namespace mlib {

////////////////////////////////////////////////////////////////////////
/// \brief DotTest Functions
////////////////////////////////////////////////////////////////////////

static void Ruby2Dot(std::vector<ExecTextExpression *> &list,
                     std::vector<ExecTextExpression *>::iterator &start,
                     const std::string &dot,
                     std::vector<ExecTextTerminal *> &dotted_list) {
  std::u16string dotted_string;
  for (const auto &elem : dotted_list) {
    dotted_string.append(elem->GetText());
  }
#if 0
  std::cout << UTF16ToUTF8(dotted_string) << std::endl;
#endif
  const auto length = dotted_list.size();
  auto end = start + length;
  for (auto it = start; it != end; ++it) {
    delete (*it);
    *it = nullptr;
  }
  start = list.erase(start, start + length);
  list.insert(start, new ExecTextWithDot(dotted_string, dot));
}

void ExecTextNonterminal::DotTest() {
  std::string dot;
  std::vector<ExecTextTerminal *> dotted_list;
  std::vector<ExecTextExpression *>::iterator it = list_.begin();
  std::vector<ExecTextExpression *>::iterator ruby_start = list_.end();
  for (; it != list_.end(); ++it) {
    ExecTextWithRuby *ruby_text = dynamic_cast<ExecTextWithRuby *>(*it);
    if (ruby_text) {
      ExecTextTerminal *terminal = dynamic_cast<ExecTextTerminal *>
                                   (ruby_text->GetExpression());
      if (terminal) {
        if (terminal->GetText().size() == 1) {
          if (dot.empty()) {
            dot = ruby_text->GetRuby();
            ruby_start = it;
          }
          if (dot == ruby_text->GetRuby()) {
            dotted_list.push_back(terminal);
            continue;
          }
        }
      }
    }
    if (dotted_list.size() >= 2) {
      Ruby2Dot(list_, ruby_start, dot, dotted_list);
      it = ruby_start;
    }
    dot.clear();
    dotted_list.clear();
    ruby_start = list_.end();
    (*it)->DotTest();
  }
  if (ruby_start != list_.end() && dotted_list.size() >= 2) {
   Ruby2Dot(list_, ruby_start, dot, dotted_list);
  }
}

void ExecTextColored::DotTest() {
  p_text_->DotTest();
}

void ExecTextWithFontSize::DotTest() {
  p_text_->DotTest();
}

void ExecTextWithRuby::DotTest() {
  p_text_->DotTest();
}

void ExecTextWithVoice::DotTest() {
  p_text_->DotTest();
}


////////////////////////////////////////////////////////////////////////
/// \brief Evaluate Functions
////////////////////////////////////////////////////////////////////////

std::string ExecTextNonterminal::Evaluate(ExecTextEvaluator *eval) {
  std::vector<std::string> string_list;
  for (auto p_exp : list_) {
    string_list.push_back(p_exp->Evaluate(eval) );
  }
  return eval->EvaluateNonterminal(string_list);
}

std::string ExecTextTerminal::Evaluate(ExecTextEvaluator *eval) {
  return eval->EvaluateTerminal(text_);
}

std::string ExecTextColored::Evaluate(ExecTextEvaluator *eval) {
  return eval->EvaluateColored(p_text_->Evaluate(eval),
                               color_red_, color_green_, color_blue_);
}

std::string ExecTextWithFontSize::Evaluate(ExecTextEvaluator *eval) {
  return eval->EvaluateWithFontSize(p_text_->Evaluate(eval), font_size_);
}

std::string ExecTextSpecial::Evaluate(ExecTextEvaluator *eval) {
  return eval->EvaluateSpecial(character_);
}

std::string ExecTextWithRuby::Evaluate(ExecTextEvaluator *eval) {
  return eval->EvaluateWithRuby(p_text_->Evaluate(eval), ruby_);
}

std::string ExecTextWithVoice::Evaluate(ExecTextEvaluator *eval) {
  return eval->EvaluateWithVoice(p_text_->Evaluate(eval), voice_);
}

std::string ExecTextWithDot::Evaluate(ExecTextEvaluator *eval) {
  return eval->EvaluateWithDot(text_, dot_);
}

////////////////////////////////////////////////////////////////////////
/// \brief ExecTextToASText Functions
////////////////////////////////////////////////////////////////////////

std::string ExecTextToASText::Evaluate(ExecTextExpression *p_exp) {
  return p_exp->Evaluate(this);
}

std::string ExecTextToASText::EvaluateNonterminal(const std::vector<std::string> &list) {
  std::string ret;
  for (const auto elem : list) {
    ret.append(elem);
  }
  return ret;
}

std::string ExecTextToASText::EvaluateTerminal(const std::u16string& text) {
  std::string ret;
  std::string ascii_text;
  for (const auto elem : text) {
    if (0 <= elem && elem <= 0xff) {
      switch (elem) {
      case '\r':
        break;
      case '\n':
        ret.append("\r\n");
        break;
      case ' ':
        if (ascii_text.empty() == false) {
          ascii_text.append(1, elem);
        } else {
          ret.append(1, elem);
        }
        break;
      case '\t':
        break;
      case '\"':
        ascii_text.append("\\\"");
        break;
      case '\\':
        ascii_text.append("\\\\");
        break;
      default:
        ascii_text.append(UTF16ToUTF8(&elem, 1));
      }
    } else {
      if (ascii_text.empty() == false) {
        ret.append(1, '"');
        ret.append(ascii_text);
        ret.append(1, '"');
        ascii_text.clear();
      }
      ret.append(UTF16ToUTF8(&elem, 1));
    }
  }
  if (ascii_text.empty() == false) {
    ret.append(1, '"');
    ret.append(ascii_text);
    ret.append(1, '"');
  }
  return ret;
}

std::string ExecTextToASText::EvaluateColored(const std::string &text,
                                              int color_red, int color_green, int color_blue) {
  std::stringstream ss;
  ss << "<FC \"#" << std::setw(2) << std::setfill('0') << std::hex
     << (color_red & 0xff) << (color_green & 0xff) << (color_blue & 0xff)
     << "\">" << text << "</FC>";
  return ss.str();
}

std::string ExecTextToASText::EvaluateWithFontSize(const std::string &text, int font_size) {
  std::stringstream ss;
  ss << "<FS " << font_size << '>' << text << "</FS>";
  return ss.str();
}

std::string ExecTextToASText::EvaluateSpecial(int character) {
  std::stringstream ss;
  ss << '%' << std::setw(2) << std::setfill('0') << character;
  return ss.str();
}

std::string ExecTextToASText::EvaluateWithRuby(const std::string &text, const std::string &ruby) {
  std::stringstream ss;
  ss << "<RB \"" << ruby << "\">" << text << "</RB>";
  return ss.str();
}

std::string ExecTextToASText::EvaluateWithVoice(const std::string &text, const std::string &voice_name) {
  std::stringstream ss;
  ss << '(' << voice_name << ')' << text;
  return ss.str();
}

std::string ExecTextToASText::EvaluateWithDot(const std::u16string &text,
                                              const std::string &dot) {
  std::stringstream ss;
  ss << "<DOT \"" << dot << "\">" << UTF16ToUTF8(text) << "</DOT>";
  return ss.str();
}

////////////////////////////////////////////////////////////////////////
/// \brief ExecTextToXhtml Functions
////////////////////////////////////////////////////////////////////////

std::string ExecTextToXhtml::GetHeader() {
  std::stringstream ss;
  ss << "<?xml version=\"1.0\" ?>\r\n"
     << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\r\n"
     << "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"ja\" lang=\"ja\">\r\n"
     << "<head><title>ExecText</title></head>\r\n"
     << "<body>\n<div style=\"writing-mode: tb-rl;\">\r\n";
  return ss.str();
}

std::string ExecTextToXhtml::GetFooter() {
  return std::string("</div>\n</body>\n</html>\n");
}

std::string ExecTextToXhtml::Evaluate(ExecTextExpression *p_exp) {
  std::stringstream ss;
  ss << "<p>" << p_exp->Evaluate(this) << "</p>\n";
  return ss.str();
}

std::string ExecTextToXhtml::EvaluateNonterminal(const std::vector<std::string> &list) {
  std::stringstream ss;
  for (const auto &elem : list) {
    ss << elem;
  }
  return ss.str();
}

std::string ExecTextToXhtml::EvaluateTerminal(const std::u16string &text) {
  std::stringstream ss;
  for (const auto &elem : text) {
    switch (elem) {
    case '\r':
      continue;
    case '\n':
      ss << "<br />\r\n";
      continue;
    default:
      ss << UTF16ToUTF8(&elem, 1);
    }
  }
  return ss.str();
}

std::string ExecTextToXhtml::EvaluateColored(const std::string &text,
                                             int color_red, int color_green, int color_blue) {
  std::stringstream ss;
  ss << "<font color=\"#" << std::setw(2) << std::setfill('0') << std::hex
     << (color_red & 0xff) << (color_green & 0xff) << (color_blue & 0xff)
     << "\">" << text << "</font>";
  return ss.str();
}

std::string ExecTextToXhtml::EvaluateWithFontSize(const std::string &text, int font_size) {
  std::stringstream ss;
  ss << "<font size=\"" << font_size << "\">" << text << "</font>";
  return ss.str();
}

std::string ExecTextToXhtml::EvaluateSpecial(int character) {
  std::stringstream ss;
  ss << "<img src=\""
     << std::setw(2) << std::setfill('0')
     << character << ".png\" alt=\"" << character << "\" />";
  return ss.str();
}

std::string ExecTextToXhtml::EvaluateWithRuby(const std::string &text, const std::string &ruby) {
  std::stringstream ss;
  ss << "<ruby><rb>" << text << "</rb><rp>(</rp><rt>"
     << ruby << "</rt><rp>)</rp></ruby>";
  return ss.str();
}

std::string ExecTextToXhtml::EvaluateWithVoice(const std::string &text, const std::string &voice_name) {
  std::stringstream ss;
  ss << '(' << voice_name << ')' << text;
  return ss.str();
}

std::string ExecTextToXhtml::EvaluateWithDot(const std::u16string &text, const std::string &dot) {
  std::stringstream ss;
  for (const auto &elem : text) {
    ss << EvaluateWithRuby(UTF16ToUTF8(&elem, 1), dot);
  }
  return ss.str();
}

////////////////////////////////////////////////////////////////////////
/// \brief Exec Functions
////////////////////////////////////////////////////////////////////////

Exec::Exec(VersionedFile *p_file)
  : p_file_(p_file), exec1_length_(0), exec2_length_(0), exec3_length_(0),
    exec4_offset_(0), exec5_offset_(0), exec6_offset_(0), exec7_offset_(0) {
  ReadExec1();
  ReadExec2();
  ReadExec3();
  CalculateExec4Offset();
  CalculateExec5Offset();
  CalculateExec6And7Offset();
}

bool Exec::ReadExec1() {
  if (p_file_ == nullptr || p_file_->IsOpened() == false) return false;
  p_file_->Seek(0, SEEK_SET);
  int exec1_count;
  p_file_->Read(4, &exec1_count);
  for (int i = 0; i < exec1_count; ++i) {
    Exec1 exec1;
    int name_len;
    p_file_->Read(4, &name_len);
    name_len &= 0x7fffffff;
    ReadString(p_file_, name_len, &exec1.name);
    p_file_->Read(4, &exec1.type);
    switch (exec1.type) {
    case Exec1::Type::kVariable:
    case Exec1::Type::kString:
      p_file_->Read(4, &exec1.size);
      p_file_->Seek(4, SEEK_CUR);
      p_file_->Read(4, &exec1.scope);
      p_file_->Seek(4, SEEK_CUR);
      p_file_->Read(4, &exec1.shift);
      p_file_->Seek(4, SEEK_CUR);
      break;
    case Exec1::Type::kArray:
      p_file_->Read(4, &exec1.size);
      p_file_->Seek(12, SEEK_CUR);
      p_file_->Read(4, &exec1.scope);
      p_file_->Seek(4, SEEK_CUR);
      p_file_->Read(4, &exec1.shift);
      p_file_->Seek(4, SEEK_CUR);
      break;
    case Exec1::Type::kFunction:
      p_file_->Seek(16, SEEK_CUR);
      p_file_->Read(4, &exec1.in_func3);
      p_file_->Seek(12, SEEK_CUR);
      break;
    default:
      continue;
    }
    exec1_.push_back(exec1);
  }
  p_file_->Read(4, &exec1_size_);
  exec1_length_ = static_cast<int>(p_file_->Seek(0, SEEK_CUR));
#if 0
  std::cout << "Exec1 length: " << exec1_length
            << ", Exec1 size: " << exec1_size_ << std::endl;
#endif
  return true;
}

bool Exec::ReadExec2() {
  if (exec1_length_ == 0) {
    if (ReadExec1() == false) return false;
  }
  p_file_->Seek(exec1_length_, SEEK_SET);
  int exec2_count;
  p_file_->Read(4, &exec2_count);
  for (int i = 0; i < exec2_count; ++i) {
    Exec2 exec2;
    int name_len;
    p_file_->Read(4, &name_len);
    name_len &= 0x7fffffff;
    ReadString(p_file_, name_len, &exec2.func_name);
    p_file_->Seek(4, SEEK_CUR);
    p_file_->Read(4, &exec2.in_func3);
    p_file_->Read(4, &exec2.func_shift);
    exec2_.push_back(exec2);
    // std::cout << "offset: " << p_file_->Seek(0, SEEK_CUR) << std::endl;
  }
  exec2_length_ = static_cast<int>(p_file_->Seek(0, SEEK_CUR));
  exec2_length_ -= exec1_length_;
#if 0
  std::cout << "Exec2 length: " << exec2_length << std::endl;
#endif
  return true;
}

bool Exec::ReadExec3() {
  if (exec2_length_ == 0) {
    if (ReadExec2() == false) return false;
  }
  p_file_->Seek(exec1_length_ + exec2_length_, SEEK_SET);
  int exec3_count;
  p_file_->Read(4, &exec3_count);
  for (int i = 0; i < exec3_count; ++i) {
    Exec3 exec3;
    int name_len;
    p_file_->Read(4, &name_len);
    name_len &= 0x7fffffff;
    ReadString(p_file_, name_len, &exec3.func_name);
    p_file_->Read(4, &exec3.func5_shift);
    exec3_.push_back(exec3);
  }
  exec3_length_ = static_cast<int>(p_file_->Seek(0, SEEK_CUR));
  exec3_length_ -= exec1_length_ + exec2_length_;
#if 0
  std::cout << "Exec3 length: " << exec3_length << std::endl;
#endif
  return true;
}

bool Exec::CalculateExec4Offset() {
  if (exec3_length_ == 0) {
    if (ReadExec3() == false) return false;
  }
  exec4_offset_ = exec1_length_ + exec2_length_ + exec3_length_;
#if 0
  std::cout << "Exec4 offset: " << exec4_offset << std::endl;
#endif
  return true;
}

bool Exec::CalculateExec5Offset() {
  if (exec4_offset_ == 0) {
    if (CalculateExec4Offset() == false) return false;
  }
  p_file_->Seek(exec4_offset_, SEEK_SET);
  int tags_len;
  p_file_->Read(4, &tags_len);
  exec5_offset_ = exec4_offset_ + 4 + tags_len;
#if 0
  std::cout << "Exec5 offset: 0x" << std::hex << exec5_offset
            << std::dec << std::endl;
#endif
  return true;
}

bool Exec::CalculateExec6And7Offset() {
  if (exec5_offset_ == 0) {
    if (CalculateExec5Offset() == false) return false;
  }
  p_file_->Seek(exec5_offset_, SEEK_SET);
  int vmcode_len;
  p_file_->Read(4, &vmcode_len);
  p_file_->Seek(vmcode_len, SEEK_CUR);
  exec6_offset_ = exec5_offset_ + 4 + vmcode_len;
  int count;
  p_file_->Read(4, &count);
  int unread_len = static_cast<int>(p_file_->Seek(0, SEEK_END)) - (exec6_offset_ + 4);
  if (unread_len <= (count * 8)) {
    exec7_offset_ = exec6_offset_;
    exec6_offset_ = exec7_offset_ + 4 + count * 4;
  } else {
    exec7_offset_ = exec6_offset_ + 4 + count * 8;
  }
#if 0
  std::cout << "Exec6 offset: 0x" << std::hex << exec6_offset << '\n'
            << "Exec7 offset: 0x" << std::hex << exec7_offset
            << std::dec << std::endl;
#endif
  return true;
}

bool Exec::ReadExec6() {
  if (exec6_offset_ == 0) {
    if (CalculateExec6And7Offset() == false) return false;
  }
  p_file_->Seek(exec6_offset_, SEEK_SET);
  int count;
  p_file_->Read(4, &count);
  int offset = 0;
  int length;
  if (exec6_offset_ < exec7_offset_) {
    for (int i = 0; i < count; ++i) {
      p_file_->Read(4, &offset);
      p_file_->Read(4, &length);
      exec6_[offset] = length;
    }
  } else {
    for (int i = 0; i < count; ++i) {
      p_file_->Read(4, &length);
      exec6_[offset] = length;
      offset += length;
    }
  }
#if 0
  for (const auto &exec6 : exec6_) {
    std::cout << exec6.first << ',' << exec6.second << std::endl;
  }
#endif
  return true;
}

bool Exec::ReadExec7() {
  if (exec6_.empty()) {
    if (ReadExec6() == false) return false;
  }
  for (const auto &exec6 : exec6_) {
    p_file_->Seek(exec7_offset_ + 4 + exec6.first, SEEK_SET);
    std::vector<char16_t> buf(exec6.second / 2);
    p_file_->Read(exec6.second, &buf[0]);
    exec7_.push_back(std::u16string(&buf[0], exec6.second / 2));
  }
  return true;
}

inline static void FlushTerminal
(std::vector<ExecTextExpression *> &l, std::u16string *text_buf) {
  if (text_buf->empty() == false) {
    l.push_back(new ExecTextTerminal(*text_buf));
    text_buf->clear();
  }
}

ExecTextExpression *Exec::ParseText(const std::u16string &text) {
  // ExecTextNonterminal *expression = new ExecTextNonterminal();
  std::vector<ExecTextExpression *> list;
  std::u16string text_buf;
  auto it = text.begin();
  auto text_end_it = text.end();
  while (it != text_end_it) {
#if 0
    std::cout << "Curr: " << it - text.begin()
              << ", Rest: " << text_end_it - it << std::endl;
#endif
    switch (*it) {
    case 1:
      FlushTerminal(list, &text_buf);
      {
        int color_red = *(++it);
        int color_green = *(++it);
        int color_blue = *(++it);
        auto end_it = ++it;
        for (; end_it != text.end(); ++end_it) {
          if (*end_it == 2) {
            if (end_it == it || *(end_it-1) != 7) break;
          }
        }
        std::u16string subtext(it, end_it);
        list.push_back(new ExecTextColored(ParseText(subtext),
                                            color_red, color_green, color_blue));
        it = end_it;
        if (it != text_end_it) ++it;
      }
      break;
    case 3:
      FlushTerminal(list, &text_buf);
      {
        int font_size = *(++it);
        auto end_it = ++it;
        for (; end_it != text.end(); ++end_it) {
          if (*end_it == 4) {
            if (end_it == it || *(end_it-1) != 7) break;
          }
        }
        std::u16string subtext(it, end_it);
        list.push_back(new ExecTextWithFontSize(ParseText(subtext),
                                                 font_size));
        it = end_it;
        if (it != text_end_it) ++it;
      }
      break;
    case 6:
      FlushTerminal(list, &text_buf);
      list.push_back(new ExecTextSpecial(*(++it)));
      ++it;
      break;
    case 7:
      switch (*(++it)) {
      case 1:
        FlushTerminal(list, &text_buf);
        {
          auto delim_it = ++it;
          for (; delim_it != text.end() && *delim_it != '\n'; ++delim_it)
            continue;
          std::u16string rb_text(it, delim_it);
          if (delim_it == text.end()) {
            list.push_back(ParseText(rb_text));
            it = text.end() - 1;
            break;
          }
          auto end_it = delim_it + 1;
          for (; end_it != text.end() && *end_it != '\0'; ++end_it)
            continue;
          std::u16string rt_text(delim_it + 1, end_it);
          rt_text.push_back(0);
          std::string rt_string = UTF16ToUTF8(&rt_text[0]);
          if (rt_string.empty()) {
            list.push_back(ParseText(rb_text));
          } else {
            list.push_back(new ExecTextWithRuby(ParseText(rb_text), rt_string));
          }
          it = end_it;
          if (it != text_end_it) ++it;
        }
        break;
      case 4:
        text_buf.push_back('\r');
        text_buf.push_back('\n');
        ++it;
        break;
      case 6:
        // interrupt waiting character
        text_buf.push_back('\r');
        text_buf.push_back('\n');
        ++it;
        break;
      case 8:
        FlushTerminal(list, &text_buf);
        {
          auto delim_it = ++it;
          for (; delim_it != text.end() && *delim_it != '\0'; ++delim_it)
            continue;
          std::u16string voice_name(it, delim_it);
          voice_name.push_back(0);
          std::string voice_name_string = UTF16ToUTF8(&voice_name[0]);
          if (delim_it == text.end()) {
            break;
          }
          auto end_it = delim_it + 1;
          for (; end_it != text.end(); ++end_it) {
            if (*end_it == 7) {
              if ((end_it+1) == text.end() || *(end_it+1) == 9) break;
            }
          }
          std::u16string voiced_text(delim_it + 1, end_it);
          list.push_back(new ExecTextWithVoice(ParseText(voiced_text), voice_name_string));
          it = end_it;
          if (it != text_end_it) ++it;
        }
        break;
      default:
        ++it;
      }
      break;
    default:
      text_buf.push_back(*it);
      ++it;
    }
#if 0
    std::cout << expression->Evaluate(new ExecTextToASText);
#endif
  }
  FlushTerminal(list, &text_buf);
  if (list.size() == 1) {
    return *list.begin();
  }
  for (auto it = list.begin(); it != list.end(); ++it) {
    ExecTextNonterminal *p_child = dynamic_cast<ExecTextNonterminal *>(*it);
    if (p_child != nullptr) {
      std::vector<ExecTextExpression *> child_list
          = p_child->TakeOutList();
      it = list.erase(it);
      delete p_child;
      it = list.insert(it, child_list.begin(), child_list.end());
    }
  }
  return new ExecTextNonterminal(std::move(list));
}

std::string Exec::ParseText(ExecTextEvaluator *p_eval) {
  if (p_eval == nullptr) return std::string();
  if (exec7_.empty()) {
    if (ReadExec7() == false) return std::string();
  }
  std::string ret;
  for (const auto &text : exec7_) {
    ExecTextExpression *exp = ParseText(text);
    exp->DotTest();
    ret.append(p_eval->Evaluate(exp));
    delete exp;
  }
  return ret;
}

void Exec::ClearParsedText() {
  while (expressions_.empty() == false) {
    ExecTextExpression* exp = expressions_.back();
    expressions_.pop_back();
    delete exp;
  }
}

} // namespace mlib
