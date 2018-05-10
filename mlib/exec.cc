/* exec.cc (updated on 2017/08/27)
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
#include <sys/stat.h>
#include <sys/types.h>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cassert>

namespace {

std::u16string ReadString16(mlib::VersionedEntry *p_file, size_t utf16_length) {
  std::u16string ret;
  ret.assign((utf16_length/2), u'\0');
  p_file->Read(utf16_length, &ret[0]);
  if (*ret.rbegin() == u'\0') ret.pop_back();
  return ret;
}

std::string CreateUUID() {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  std::srand(std::time(nullptr));
  unsigned int tmp = (std::rand() << 17) | ((std::rand() & 0x7fff) << 2) | (std::rand() & 0x03);
  oss << std::setw(8) << tmp << '-';
  tmp = ((std::rand() & 0xff) << 8) | (std::rand() & 0xff);
  oss << std::setw(4) << tmp << '-';
  tmp = std::rand() & 0x0fff;
  oss << '4' << std::setw(3) << tmp << '-';
  tmp = (2 << 30) | ((std::rand() & 0x7fff) << 15) | (std::rand() & 0x7fff);
  oss << std::setw(4) << tmp << '-';
  tmp = ((std::rand() & 0x0fff) << 12) | (std::rand() & 0x0fff);
  oss << std::setw(3) << tmp;
  tmp = ((std::rand() & 0x0fff) << 12) | (std::rand() & 0x0fff);
  oss << std::setw(3) << tmp;
  return oss.str();
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
  assert(eval != nullptr);
  std::vector<std::string> string_list;
  for (auto p_exp : list_) {
    assert(p_exp != nullptr);
    string_list.push_back(p_exp->Evaluate(eval));
  }
  return eval->EvaluateNonterminal(string_list);
}

std::string ExecTextTerminal::Evaluate(ExecTextEvaluator *eval) {
  assert(eval != nullptr);
  return eval->EvaluateTerminal(text_);
}

std::string ExecTextColored::Evaluate(ExecTextEvaluator *eval) {
  assert(eval != nullptr);
  return eval->EvaluateColored(p_text_->Evaluate(eval),
                               color_red_, color_green_, color_blue_);
}

std::string ExecTextWithFontSize::Evaluate(ExecTextEvaluator *eval) {
  assert(eval != nullptr);
  return eval->EvaluateWithFontSize(p_text_->Evaluate(eval), font_size_);
}

std::string ExecTextSpecial::Evaluate(ExecTextEvaluator *eval) {
  assert(eval != nullptr);
  return eval->EvaluateSpecial(character_);
}

std::string ExecTextWaitingForKeyIn::Evaluate(ExecTextEvaluator* eval) {
  assert(eval != nullptr);
  return eval->EvaluateWaitingForKeyIn();
}

std::string ExecTextWithRuby::Evaluate(ExecTextEvaluator *eval) {
  assert(eval != nullptr);
  return eval->EvaluateWithRuby(p_text_->Evaluate(eval), ruby_);
}

std::string ExecTextWithVoice::Evaluate(ExecTextEvaluator *eval) {
  assert(eval != nullptr);
  return eval->EvaluateWithVoice(p_text_->Evaluate(eval), voice_);
}

std::string ExecTextWithDot::Evaluate(ExecTextEvaluator *eval) {
  assert(eval != nullptr);
  return eval->EvaluateWithDot(text_, dot_);
}

////////////////////////////////////////////////////////////////////////
/// \brief ExecTextEvaluator Functions
////////////////////////////////////////////////////////////////////////

ExecTextEvaluator::ExecTextEvaluator() : novel_mode_(false) {}

void ExecTextEvaluator::EvaluateTag(const std::string& tag_name,
                                    const std::map<std::string, std::string>& attrs) {
#if 0
  std::cout << "Tag name: " << tag_name << " (";
  for (const auto& attr : attrs) {
    std::cout << attr.first << "='" << attr.second << "' ";
  }
  std::cout << '\b' << ')' << std::endl;
#endif
  if (tag_name == "msgframe") {
    auto src_itr = attrs.find("src");
    auto visibility_itr = attrs.find("visibility");
    const std::string src = src_itr != attrs.end() ? src_itr->second : std::string();
    const std::string visibility = visibility_itr != attrs.end() ? visibility_itr->second
                                                                 : std::string();
    if (src.empty() == false) {
      if (src.find("novel") != std::string::npos) {
        OnChangeMsgFrame(src, visibility);
        novel_mode_ = true;
        OnSwitchedNovelMode(novel_mode_);
      } else {
        novel_mode_ = false;
        OnSwitchedNovelMode(novel_mode_);
        OnChangeMsgFrame(src, visibility);
      }
    } else {
      OnChangeMsgFrame(src, visibility);
    }
    return;
  }
  if (tag_name == "cg") {
    auto src_itr = attrs.find("src");
    auto mask_itr = attrs.find("mask");
    auto time_itr = attrs.find("time");
    auto msg_hide_itr = attrs.find("msg-hide");
    OnChangedCG(src_itr != attrs.end() ? src_itr->second : std::string(),
                mask_itr != attrs.end() ? mask_itr->second : std::string(),
                time_itr != attrs.end() ? time_itr->second : std::string(),
                (msg_hide_itr != attrs.end() && msg_hide_itr->second == "false")
                ? false : true);  // TODO: case-insensitive compare
    return;
  }
  if (tag_name == "char") {
    auto na_itr = attrs.find("na");
    if (na_itr != attrs.end()) {
      auto dr_itr = attrs.find("dr");
      auto ex_itr = attrs.find("ex");
      auto pl_itr = attrs.find("pl");
      auto tm_itr = attrs.find("tm");
      OnDisplayedChara(na_itr->second,
                       dr_itr != attrs.end() ? dr_itr->second : std::string(),
                       ex_itr != attrs.end() ? ex_itr->second : std::string(),
                       pl_itr != attrs.end() ? pl_itr->second : std::string(),
                       tm_itr != attrs.end() ? tm_itr->second : std::string());
    } else {
      auto src_itr = attrs.find("src");
      auto pos_itr = attrs.find("position");
      if (pos_itr == attrs.end()) pos_itr = attrs.find("pos");
      auto time_itr = attrs.find("time");
      auto opacity_itr = attrs.find("opacity");
      OnDisplayedChara(src_itr != attrs.end() ? src_itr->second : std::string(),
                       pos_itr != attrs.end() ? pos_itr->second : std::string(),
                       time_itr != attrs.end() ? time_itr->second : std::string(),
                       opacity_itr != attrs.end() ? opacity_itr->second : std::string());
    }
    return;
  }
  if (tag_name == "charclaer") {
    auto na_itr = attrs.find("na");
    OnErasedChara(na_itr != attrs.end() ? na_itr->second : std::string());
    return;
  }
}

void ExecTextEvaluator::EvaluateLabel(const std::u16string& label) {
  std::string new_higher_label = UTF16ToUTF8(label);
  label_ = new_higher_label;
  auto it = new_higher_label.begin();
  do {
    bool m = false;
    for (; it != new_higher_label.end(); ++it) {
      // std::cout << *it;
      if (isalpha(*it)) {
        m = true;
        continue;
      }
      if (it == new_higher_label.begin()) {
        m = false;
      }
      break;
    }
    if (m == false) {
      // new_label_main.clear();
      break;
    }
    // std::cout << '|';
    m = false;
    for (; it != new_higher_label.end(); ++it) {
      // std::cout << *it;
      if (isdigit(*it)) {
        m = true;
        continue;
      }
      break;
    }
    if (m == true) {
      break;
    }
    // std::cout << '|';
    for (; it != new_higher_label.end(); ++it) {
      // std::cout << *it;
      if (*it != '_') break;
    }
    m = false;
    // std::cout << '|';
    for (; it != new_higher_label.end(); ++it) {
     // std::cout << *it;
      if (*it == '_') {
        m = true;
        break;
      }
    }
    if (m == false) break;
    auto it2 = it;
    m = false;
    for (++it2; it2 != new_higher_label.end(); ++it2) {
      if (*it2 == '_') break;
      if (isdigit(*it2) == 0) break;
      m = true;
    }
    if (m == true) {
      it = it2;
    }
    // std::cout << std::endl;
  } while (false);
  new_higher_label.erase(it, new_higher_label.end());
#if 0
  std::cout << "Label name: " << label_ << std::endl;
#endif
  if (new_higher_label != higher_label_ && new_higher_label.empty() == false) {
 #if 0
    std::cout << "Higher label name: " << new_higher_label << std::endl;
 #endif
    higher_label_ = new_higher_label;
    OnChangedHigherLabel(new_higher_label);
    OnChangedLowerLabel(label_, true);
  } else {
    OnChangedLowerLabel(label_, false);
  }
}

void ExecTextEvaluator::EvaluateCharaName(const std::u16string& name) {
  chara_name_ = UTF16ToUTF8(name);
}

std::string ExecTextEvaluator::PopCharaName() {
  std::string ret = chara_name_;
  chara_name_.clear();
  return ret;
}

////////////////////////////////////////////////////////////////////////
/// \brief ExecTextToASText Functions
////////////////////////////////////////////////////////////////////////

ExecTextToASText::ExecTextToASText(const std::string& file_name)
  : ofs_(file_name) {}

std::string ExecTextToASText::Evaluate(ExecTextExpression *p_exp) {
  assert(p_exp != nullptr);
  std::string ret = p_exp->Evaluate(this);
  auto len = ret.length();
  if (len >= 2 && ret[len-2] == '$' && ret[len-1] == 'k') {
    ret[len-2] = '\r';
    ret[len-1] = '\n';
    if (IsNovelMode()) {
      ret.append("$p\r\n");
    }
  }
  if (IsNovelMode() == false && GetCharaName().empty() == false) {
    std::string chara_name(1, '#');
    chara_name.append(PopCharaName());
    chara_name.append(1, ' ');
    ret.insert(0, chara_name);
  }
  if (ofs_.is_open()) {
    ofs_ << ret << "\r\n";
    ofs_.flush(); // for debug
  }
  return ret;
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
        ret.append("$e");
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

std::string ExecTextToASText::EvaluateWaitingForKeyIn() {
  return "$k";
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

void ExecTextToASText::StartSelectMode() {
  option_count_ = 0;
  if (ofs_.is_open()) {
    ofs_ << "&cjump (";
  }
}

void ExecTextToASText::AddOption(const std::string& option, const std::string& label, const std::string& cond) {
  if (ofs_.is_open()) {
    if (option_count_ > 0) {
      ofs_ << ", ";
    }
    ofs_ << '"' << option << "\", " << label << ", "
         << (cond.empty() ? "1" : cond.c_str());
  }
  ++option_count_;
}

void ExecTextToASText::EndSelectMode() {
  if (ofs_.is_open()) {
    ofs_ << ")\r\n\r\n";
  }
}

void ExecTextToASText::SetParameter(const std::string& exp) {
  if (ofs_.is_open()) {
    ofs_ << "&parameter " << exp << " ;\r\n";
  }
}

void ExecTextToASText::Jump(const std::string& label, const std::string& cond) {
  if (ofs_.is_open()) {
    if (cond.empty() == false) {
      ofs_ << "&if " << cond << " then ";
    }
    ofs_ << "&goto " << label << " ;\r\n\r\n";
  }
}

void ExecTextToASText::OnSwitchedNovelMode(bool novel_mode) {
  if (novel_mode) {
     if (ofs_.is_open()) {
       ofs_ << "<novel>\r\n";
     }
  } else {
    if (ofs_.is_open()) {
      ofs_ << "</novel>\r\n";
    }
  }
}

void ExecTextToASText::OnChangedLowerLabel(const std::string& new_label, bool) {
  if (ofs_.is_open()) {
    ofs_ << new_label << ":\r\n";
  }
}

void ExecTextToASText::OnChangedCG(const std::string& src, const std::string& mask,
                                   const std::string& display_time, bool msg_hide) {
  if (ofs_.is_open()) {
    ofs_ << "<cg";
    if (src.empty() == false) {
      ofs_ << " src='" << src << '\'';
    }
    if (mask.empty() == false) {
      ofs_ << " mask='" << mask << '\'';
    }
    if (display_time.empty() == false) {
      ofs_ << " time='" << display_time << '\'';
    }
    if (msg_hide == false) {
      ofs_ << " msg-hide='false'";
    }
    ofs_ << ">\r\n";
  }
}

void ExecTextToASText::OnDisplayedChara(const std::string& na, const std::string& dr,
                                        const std::string& ex, const std::string& pl,
                                        const std::string& tm) {
  if (ofs_.is_open()) {
#if 0
    ofs_ << "<char";
    if (na.empty() == false) {
      ofs_ << " na='" << na << '\'';
    }
    if (dr.empty() == false) {
      ofs_ << " dr='" << dr << '\'';
    }
    if (ex.empty() == false) {
      ofs_ << " ex='" << ex << '\'';
    }
    if (pl.empty() == false) {
      ofs_ << " pl='" << pl << '\'';
    }
    if (tm.empty() == false) {
      ofs_ << " tm='" << tm << '\'';
    }
    ofs_ << ">\r\n";
#else
    if (na.empty()) {
      throw std::invalid_argument("nm is empty");
    }
    ofs_ << "&char " << na << ' ';
    if (ex.empty() == false) {
      ofs_ << ex << ' ';
    }
    if (dr.empty() == false) {
      ofs_ << dr << ' ';
    }
    if (pl.empty() == false) {
      ofs_ << pl << ' ';
    }
    if (tm.empty() == false && tm != "350") {
      ofs_ << tm << ' ';
    }
    ofs_ << ";\r\n";
#endif
  }
}

void ExecTextToASText::OnDisplayedChara(const std::string& src, const std::string& position,
                                        const std::string& display_time, const std::string& opacity) {
  if (ofs_.is_open()) {
    ofs_ << "<char";
    if (src.empty() == false) {
      ofs_ << " src='" << src << '\'';
    }
    if (position.empty() == false) {
      ofs_ << " pos='" << position << '\'';
    }
    if (display_time.empty() == false) {
      ofs_ << " time='" << display_time << '\'';
    }
    if (opacity.empty() == false) {
      ofs_ << " opacity='" << opacity << '\'';
    }
    ofs_ << ">\r\n";
  }
}

void ExecTextToASText::OnErasedChara(const std::string& na) {
  if (ofs_.is_open()) {
    ofs_ << "&charclear ";
    if (na.empty() == false) {
      ofs_ << na << ' ';
    }
    ofs_ << ";\r\n";
  }
}

void ExecTextToASText::OnChangeMsgFrame(const std::string& src, const std::string& visibility) {
  if (ofs_.is_open()) {
    ofs_ << "<msgframe";
    if (src.empty() == false) {
      ofs_ << " src='" << src << '\'';
    }
    if (visibility.empty() == false) {
      ofs_ << " visibility='" << visibility << '\'';
    }
    ofs_ << ">\r\n";
  }
}

////////////////////////////////////////////////////////////////////////
/// \brief ExecTextToXhtml Functions
////////////////////////////////////////////////////////////////////////

ExecTextToXhtml::ExecTextToXhtml(const std::string& product, bool /*vertical*/)
  : product_name_(product), base_dir_("."), p_ofs_(nullptr), curr_size_(0), curr_color_(0xffffff) {
  ::mkdir(base_dir_.c_str(), 0755);
  // remove all files and directories in XXX.ebook directory
#ifdef _WINDOWS
  std::string rm_file_cmd("del /S /Q ");
  rm_file_cmd.append(base_dir_);
  ::system(rm_file_cmd.c_str());
  std::string rm_dir_cmd("for /D %%1 in (");
  rm_dir_cmd.append(base_dir_).append(") do rmdir /S /Q \"%%1\"");
  ::system(rm_dir_cmd.c_str());
#else
#if 0
  std::string rm_cmd("rm -rf ");
  std::string rm_target(base_dir_);
  rm_target.append(1, kPathDelim).append(1, '*');
  if (rm_target[0] == kPathDelim) {
    throw std::invalid_argument("FATAL ERROR: tried to remove all files and directories in the root directory.");
  }
  rm_cmd.append(rm_target);
  ::system(rm_cmd.c_str());
#endif
#endif
  // create mimetype
  std::string mimetype_filename(base_dir_);
  mimetype_filename.append(1, kPathDelim).append("mimetype");
  std::ofstream mimetype_fs(mimetype_filename);
  mimetype_fs << "application/epub+zip";
  mimetype_fs.close();
  // create META-INF/container.xml
  std::string meta_dirname(base_dir_);
  meta_dirname.append(1, kPathDelim).append("META-INF");
  ::mkdir(meta_dirname.c_str(), 0755);
  std::string container_filename(meta_dirname);
  container_filename.append(1, kPathDelim).append("container.xml");
  std::ofstream container_fs(container_filename);
  container_fs << "<?xml version=\"1.0\"?>\r\n"
               << "<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\r\n"
               << "  <rootfiles>\r\n"
               << "    <rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\" />\r\n"
               << "  </rootfiles>\r\n"
               << "</container>";
  container_fs.close();
  // create OEBPS
  std::string oebps_dirname(base_dir_);
  oebps_dirname.append(1, kPathDelim).append("OEBPS");
  ::mkdir(oebps_dirname.c_str(), 0755);
}

ExecTextToXhtml::~ExecTextToXhtml() {
/*
  if (p_ofs_ != nullptr && p_ofs_->is_open()) {
    *p_ofs_ << GetFooter();
    p_ofs_->close();
    delete p_ofs_;
  }
*/
  for (auto it = xhtmls_.begin(); it != xhtmls_.end(); ) {
    if (it->second->is_open()) {
      *(it->second) << GetFooter();
      it->second->close();
    }
    delete it->second;
    it = xhtmls_.erase(it);
  }
  // create OEBPS/content.opf
  std::string oebps_dirname(base_dir_);
  oebps_dirname.append(1, kPathDelim).append("OEBPS");
  std::string content_opf_filename(oebps_dirname);
  content_opf_filename.append(1, kPathDelim).append("content.opf");
  std::ofstream content_opf_fs(content_opf_filename);
  content_opf_fs << "<?xml version='1.0' encoding='utf-8'?>\r\n"
                 << "<package xmlns=\"http://www.idpf.org/2007/opf\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" unique-identifier=\"bookid\" version=\"3.0\">\r\n"
                 << "  <metadata>\r\n"
                 << "    <dc:title>" << product_name_ << "</dc:title>\r\n"
                 << "    <dc:creator>renny1398</dc:creator>\r\n";
  uuid_ = CreateUUID();
  content_opf_fs << "    <dc:identifier id=\"bookid\">urn:uuid:" << uuid_ << "</dc:identifier>\r\n"
                 << "    <dc:language>ja-JP</dc:language>\r\n";
  time_t timer;
  struct tm* t_st;
  ::time(&timer);
  t_st = localtime(&timer);
  content_opf_fs << "    <meta property=\"dcterms:modified\">" << std::setfill('0') << std::setw(4) << t_st->tm_year + 1900 << '-'
                 << std::setw(2) << t_st->tm_mon + 1 << '-' << std::setw(2) << t_st->tm_mday << 'T'
                 << std::setw(2) << t_st->tm_hour << ':' << std::setw(2) << t_st->tm_min << ':' << std::setw(2) << t_st->tm_sec << "Z</meta>\r\n";
  content_opf_fs << "    <meta name=\"primary-writing-mode\" content=\"vertical-rl\"/>\r\n";
  content_opf_fs << "  </metadata>\r\n"
                 << "  <manifest>\r\n";
  content_opf_fs << "    <item id=\"ncx\" href=\"toc.ncx\" media-type=\"application/x-dtbncx+xml\"/>\r\n";
  for (const auto& content_id : content_order_) {
    content_opf_fs << "    <item id=\"" << content_id << "\" href=\"" << content_id << ".xhtml\" media-type=\"application/xhtml+xml\"/>\r\n";
  }
  content_opf_fs << "    <item id=\"toc\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/>\r\n";
  content_opf_fs << "    <item id=\"stylesheet.css\" href=\"stylesheet.css\" media-type=\"text/css\"/>\r\n";
  // TODO: add image files
  content_opf_fs << "  </manifest>\r\n"
                 << "  <spine toc=\"ncx\" page-progression-direction=\"rtl\">\r\n"
                 << "    <itemref idref=\"toc\" linear=\"no\"/>\r\n";
  for (const auto& content_id : content_order_) {
    content_opf_fs << "    <itemref idref=\"" << content_id << "\"/>\r\n";
  }
  content_opf_fs << "  </spine>\r\n"
                 << "</package>";
  content_opf_fs.close();
  // create OEBPS/toc.ncx
  std::string toc_filename(oebps_dirname);
  toc_filename.append(1, kPathDelim).append("toc.ncx");
  std::ofstream toc_fs(toc_filename);
  toc_fs << "<?xml version='1.0' encoding='utf-8'?>\r\n"
         << "<ncx xmlns=\"http://www.daisy.org/z3986/2005/ncx/\" version=\"2005-1\">\r\n"
         << "  <head>\r\n"
         << "    <meta name=\"dtb:uid\" content=\"urn:uuid:" << uuid_ << "\"/>\r\n"
         << "    <meta name=\"dtb:depth\" content=\"1\"/>\r\n"
         << "    <meta name=\"dtb:totalPageCount\" content=\"0\"/>\r\n"
         << "    <meta name=\"dtb:maxPageNumber\" content=\"0\"/>\r\n"
         << "  </head>\r\n"
         << "  <docTitle>\r\n"
         << "    <text>" << product_name_ << "</text>\r\n"
         << "  </docTitle>\r\n"
         << "  <navMap>\r\n";
  for (size_t i = 0; i < content_order_.size(); ++i) {
    toc_fs << "    <navPoint id=\"navpoint-" << i + 1 << "\" playOrder=\"" << i + 1 << "\">\r\n"
           << "      <navLabel>\r\n"
           << "        <text>" << content_order_[i] << "</text>\r\n"
           << "      </navLabel>\r\n"
           << "      <content src=\"" << content_order_[i] << ".xhtml\"/>\r\n"
           << "    </navPoint>\r\n";
  }
  toc_fs << "  </navMap>\r\n"
         << "</ncx>";
  toc_fs.close();
  // create OEBPS/nav.xhtml
  std::string nav_filename(oebps_dirname);
  nav_filename.append(1, kPathDelim).append("nav.xhtml");
  std::ofstream nav_fs(nav_filename);
  nav_fs << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
         << "<!DOCTYPE html>\r\n"
         << "<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\" xml:lang=\"ja\">\r\n"
         << "<head>\r\n"
         << "  <meta charset=\"utf-8\" />\r\n"
         << "  <link href=\"stylesheet.css\" rel=\"stylesheet\" type=\"text/css\"/></head>\r\n"
         << "<body epub:type=\"frontmatter\">\r\n"
         << "  <nav epub:type=\"toc\" id=\"toc\">\r\n"
         << "    <h1>Table of Contents</h1>\r\n"
         << "    <ol>\r\n";
  for (const auto& content_id : content_order_) {
    nav_fs << "      <li>\r\n"
           << "        <a href=\"" << content_id << ".xhtml\">" << content_id << "</a>\r\n"
           << "      </li>\r\n";
  }
  nav_fs << "    </ol>\r\n"
         << "  </nav>\r\n"
         << "</body>\r\n"
         << "</html>\r\n";
  nav_fs.close();
  // create stylesheet.css
  std::string css_filename(oebps_dirname);
  css_filename.append(1, kPathDelim).append("stylesheet.css");
  std::ofstream css_fs(css_filename);
  css_fs << "@charset \"UTF-8\";\r\n\r\n"
         << "body {\r\n"
         // << "  line-height: 1.5em;\r\n"
         << "  writing-mode: vertical-rl;\r\n"
         << "  line-break: normal;\r\n"
         << "  -epub-writing-mode: vertical-rl;\r\n"
         << "  -webkit-writing-mode: vertical-rl;\r\n"
         << "  -epub-line-break: normal;\r\n"
         << "  -webkit-line-break: normal;\r\n"
         << "}\r\n\r\n"
         << ".normal {}\r\n"
         << ".novel {}";
  css_fs.close();
#ifndef _WINDOWS
  std::string zip_filename(product_name_);
  zip_filename.append(".epub");
  std::string zip_cmd("zip -0Xq ");
  zip_cmd.append(zip_filename).append(1, ' ').append(base_dir_).append(1, kPathDelim).append("mimetype");
  ::system(zip_cmd.c_str());
  zip_cmd.clear();
  zip_cmd.assign("zip -Xr9Dq ").append(zip_filename).append(1, ' ').append(base_dir_).append(1, kPathDelim).append("META-INF");
  ::system(zip_cmd.c_str());
  zip_cmd.clear();
  zip_cmd.assign("zip -Xr9Dq ").append(zip_filename).append(1, ' ').append(base_dir_).append(1, kPathDelim).append("OEBPS");
  ::system(zip_cmd.c_str());
#endif
}

static void OutputFontStartTag(std::ostream* os, int size, int color) {
  assert(os != nullptr);
  *os << "<font";
  if (size != 0) {
    *os << " size=\"" << size << '"';
  }
  if ((color & 0xffffff) != 0xffffff) {
    *os << " color=\"#" << std::setfill('0') << std::hex
        << std::setw(2) << (color & 0xff)
        << std::setw(2) << ((color >> 8) & 0xff)
        << std::setw(2) << ((color >> 16) & 0xff)
        << '"' << std::setfill(' ') << std::dec;
  }
  *os << '>';
}

std::string ExecTextToXhtml::GetHeader() {
  std::stringstream ss;
  ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
     << "<!DOCTYPE html>\r\n"
     << "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"ja\">\r\n"
     << "<head>\r\n"
     << "<title>" << GetHigherLabel() << "</title>\r\n"
     << "<link href=\"stylesheet.css\" rel=\"stylesheet\" type=\"text/css\"/>\r\n"
     << "</head>\r\n"
     << "<body>\r\n"
     << "<div class=\"" << (IsNovelMode() ? "novel" : "normal") << "\">\r\n";
  return ss.str();
}

std::string ExecTextToXhtml::GetFooter() {
  return std::string("</div>\r\n</body>\r\n</html>\r\n");
}

std::string ExecTextToXhtml::Evaluate(ExecTextExpression *p_exp) {
  assert(p_exp != nullptr);
  std::ostringstream ss;
  ss << "<p>";
  if (curr_color_ != 0xffffff/* || curr_size_ != 0*/) {
    OutputFontStartTag(&ss, curr_size_, curr_color_);
  }
  if (IsNovelMode() == false) {
    if (GetCharaName().empty() == false) {
      ss << PopCharaName();
    } else {
      ss << "　";
    }
  }
  ss << p_exp->Evaluate(this);
  if (curr_color_ != 0xffffff/* || curr_size_ != 0*/) {
    ss << "</font>";
  }
  ss << "</p>\r\n";
  if (p_ofs_ != nullptr && p_ofs_->is_open()) {
    *p_ofs_ << ss.str();
  }
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
  std::ostringstream ss;
  OutputFontStartTag(&ss, 0, (color_red & 0xff) | ((color_green & 0xff) << 8) |
                             ((color_blue & 0xff) << 16));
  ss << text << "</font>";
  return ss.str();
}

std::string ExecTextToXhtml::EvaluateWithFontSize(const std::string &text, int font_size) {
  std::stringstream ss;
  OutputFontStartTag(&ss, font_size, -1);
  ss << text << "</font>";
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
  ss << "<ruby>" << text << "<rp>(</rp><rt>"
     << ruby << "</rt><rp>)</rp></ruby>";
  return ss.str();
}

std::string ExecTextToXhtml::EvaluateWithVoice(const std::string &text, const std::string &voice_name) {
  std::stringstream ss;
  // if (novel_mode_ == true) {
    ss << '(' << voice_name << ')';
  // }
  ss << text;
  return ss.str();
}

std::string ExecTextToXhtml::EvaluateWithDot(const std::u16string &text, const std::string &dot) {
  std::stringstream ss;
  for (const auto &elem : text) {
    ss << EvaluateWithRuby(UTF16ToUTF8(&elem, 1), dot);
  }
  return ss.str();
}

void ExecTextToXhtml::OnSwitchedNovelMode(bool novel_mode) {
  if (p_ofs_ != nullptr && p_ofs_->is_open()) {
    *p_ofs_ << "</div>\r\n<div class=\"" << (novel_mode ? "novel" : "normal")
            << "\">\r\n";
  }
}

void ExecTextToXhtml::OnChangedHigherLabel(const std::string& new_label) {
  if (new_label.empty() == false && new_label[0] == '$') return;
  auto it = xhtmls_.find(new_label);
  if (it == xhtmls_.end()) {
    std::string xhtml_filename(base_dir_);
    xhtml_filename.append(1, kPathDelim).append("OEBPS").append(1, kPathDelim).append(new_label).append(".xhtml");
    p_ofs_ = new std::ofstream(xhtml_filename);
    if (p_ofs_->is_open() == false) {
      std::error_code ec(static_cast<int>(std::errc::read_only_file_system),
                         std::generic_category());
      throw std::system_error(ec, "failed to open a file");
    }
    *p_ofs_ << GetHeader();
    xhtmls_.insert(std::make_pair(new_label, p_ofs_));
    content_order_.push_back(new_label);
  } else {
    p_ofs_ = it->second;
  }
}

void ExecTextToXhtml::OnChangedLowerLabel(const std::string& new_label, bool changed_higher_label) {
  if (new_label.empty() == false && new_label[0] == '$') return;
  if (changed_higher_label == false) {
    if (p_ofs_ != nullptr && p_ofs_->is_open()) {
      *p_ofs_ << "<p><!-- " << new_label << " --><br /></p>\r\n";
    }
  }
}

void ExecTextToXhtml::StartSelectMode() {
  if (p_ofs_ != nullptr && p_ofs_->is_open()) {
    *p_ofs_ << "<div><ol>";
  }
}

void ExecTextToXhtml::AddOption(const std::string& option, const std::string&, const std::string&) {
  if (p_ofs_ != nullptr && p_ofs_->is_open()) {
    *p_ofs_ << "<li>[" << option << "]</li>";
  }
}

void ExecTextToXhtml::EndSelectMode() {
  if (p_ofs_ != nullptr && p_ofs_->is_open()) {
    *p_ofs_ << "</ol></div>\r\n";
  }
}

void ExecTextToXhtml::ChangeSize(int new_size) {
  // if (new_size == curr_size_) return;
  curr_size_ = new_size;
}

void ExecTextToXhtml::ChangeColor(int new_color) {
  // if (new_color == curr_color_) return;
  curr_color_ = new_color;
}

////////////////////////////////////////////////////////////////////////
/// \brief Exec Functions
////////////////////////////////////////////////////////////////////////

Exec::Exec(VersionedEntry *p_file, const std::string& product)
  : p_file_(p_file), product_(product),
    exec1_length_(0), exec2_length_(0), exec3_length_(0),
    exec4_offset_(0), exec5_offset_(0), exec6_offset_(0), exec7_offset_(0) {
  ReadExec1();
  ReadExec2();
  ReadExec3();
  ReadExec4();
  ReadExec5();
  CalculateExec6And7Offset();
}

bool Exec::ReadExec1() {
  if (p_file_ == nullptr || p_file_->IsFile() == false) return false;
  p_file_->Seek(0, SEEK_SET);
  int exec1_count;
  p_file_->Read(4, &exec1_count);
  // std::cout << "Exec1 count: " << exec1_count << std::endl;
  for (int i = 0; i < exec1_count; ++i) {
    Exec1 exec1;
    int name_len;
    p_file_->Read(4, &name_len);
    if ((name_len & 0x80000000) == 0) {
      p_file_->Seek(-4, SEEK_CUR);
      break;
    }
    name_len &= 0x7fffffff;
    exec1.name = ReadString16(p_file_, name_len);
    p_file_->Read(4, &exec1.type);
    switch (exec1.type) {
    case Exec1::Type::kVariable:
      if (exec1.name == u"f") {
        exec1.init_value = 1;
      } else {
        exec1.init_value = 0;
      }
      exec1.value = exec1.init_value;
      /* FALLTHRU */
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
      exec1.size = 0;
      exec1.scope = 0;
      exec1.shift = 0;
      break;
    default:
      std::cerr << "Unknown type: " << exec1.type << std::endl;
      continue;
    }
    exec1_.push_back(exec1);
  }
  p_file_->Read(4, &exec1_size_);
  exec1_length_ = static_cast<int>(p_file_->Seek(0, SEEK_CUR));
#if 0
  std::cout << "Exec1 length: " << exec1_length_
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
    exec2.func_name = ReadString16(p_file_, name_len);
    p_file_->Read(4, &exec2.id);
    p_file_->Read(4, &exec2.in_label_block);
    p_file_->Read(4, &exec2.code_offset);
    exec2_.push_back(exec2);
    switch (exec2.in_label_block) {
    case 0: // external function
      ext_funcs_.insert(std::make_pair(exec2.id, FuncInfo(exec2.func_name, exec2.code_offset)));
      break;
    default: // internal function
      int_funcs_.insert(std::make_pair(exec2.id, FuncInfo(exec2.func_name, exec2.code_offset)));
      break;
    }
  }
  exec2_length_ = static_cast<int>(p_file_->Seek(0, SEEK_CUR));
  exec2_length_ -= exec1_length_;
#if 0
  std::cout << "Exec2 length: " << exec2_length_ << std::endl;
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
    // Exec3 exec3;
    std::pair<int, std::u16string> exec3;
    int name_len;
    p_file_->Read(4, &name_len);
    name_len &= 0x7fffffff;
    exec3.second = ReadString16(p_file_, name_len);
    p_file_->Read(4, &exec3.first);
    exec3_.insert(exec3);
    if (GetInternalFuncId(exec3.second) == -1) {
      if (exec3.second[0] == u'$') {
        tmp_lbls_.insert(std::make_pair(exec3.first, exec3.second));
      } else {
        labels_.insert(std::make_pair(exec3.first, exec3.second));
      }
    }
  }
  exec3_length_ = static_cast<int>(p_file_->Seek(0, SEEK_CUR));
  exec3_length_ -= exec1_length_ + exec2_length_;
#if 0
  std::cout << "Exec3 length: " << exec3_length_ << std::endl;
#endif
  return true;
}

bool Exec::ReadExec4() {
  if (exec3_length_ == 0) {
    if (ReadExec3() == false) return false;
  }
  exec4_offset_ = exec1_length_ + exec2_length_ + exec3_length_;
  p_file_->Seek(exec4_offset_, SEEK_SET);
  int exec4_len;
  p_file_->Read(4, &exec4_len);
  vmdata_.assign(exec4_len / sizeof(char16_t), 0);
  p_file_->Read(exec4_len, &vmdata_[0]);
#if 0
  std::cout << "Exec4 offset: " << exec4_offset_ << std::endl;
#endif
  exec5_offset_ = exec4_offset_ + 4 + exec4_len;
  return true;
}

bool Exec::ReadExec5() {
  if (exec4_offset_ == 0 || exec5_offset_ == 0) {
    if (ReadExec4() == false) return false;
  }
  p_file_->Seek(exec5_offset_, SEEK_SET);
  int exec5_len;
  p_file_->Read(4, &exec5_len);
  vmcode_.assign(exec5_len, 0);
  p_file_->Read(exec5_len, &vmcode_[0]);
#if 0
  std::cout << "Exec5 offset: 0x" << std::hex << exec5_offset_
            << std::dec << std::endl;
#endif
  return true;
}

bool Exec::CalculateExec6And7Offset() {
  if (exec5_offset_ == 0) {
    if (ReadExec5() == false) return false;
  }
  p_file_->Seek(exec5_offset_, SEEK_SET);
  int vmcode_len;
  p_file_->Read(4, &vmcode_len);
  p_file_->Seek(vmcode_len, SEEK_CUR);
  exec6_offset_ = exec5_offset_ + 4 + vmcode_len;
  int count;
  p_file_->Read(4, &count);
  // std::cout << "Exec6 count: 0x" << std::hex << count << '\n';
  int unread_len = static_cast<int>(p_file_->Seek(0, SEEK_END)) - (exec6_offset_ + 4);
  // std::cout << "Unread length: 0x" << std::hex << unread_len << '\n';
  if (unread_len <= (count * 8)) {
    exec7_offset_ = exec6_offset_;
    exec6_offset_ = exec7_offset_ + 4 + count;
  } else {
    exec7_offset_ = exec6_offset_ + 4 + count * 8;
  }
#if 0
  std::cout << "Exec6 offset: 0x" << std::hex << exec6_offset_ << '\n'
            << "Exec7 offset: 0x" << std::hex << exec7_offset_
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
  int offset = 0;
  int length;
  p_file_->Read(4, &count);
  // std::cout << "Exec6 count: " << count << std::endl;
  if (exec6_offset_ < exec7_offset_) {
    for (int i = 0; i < count; ++i) {
      p_file_->Read(4, &offset);
      p_file_->Read(4, &length);
      exec6_[offset] = length;
    }
  } else {
    // off_t curr_ofs = exec6_offset_ + 4;
    // auto file_size = p_file_->Seek(0, SEEK_END);
    // p_file_->Seek(curr_ofs, SEEK_SET);
    int next_ofs;
    p_file_->Read(4, &offset);
    // curr_ofs += 4;
    for (int i = 0; i < count - 1; ++i) {
      p_file_->Read(4, &next_ofs);
      // curr_ofs += 4;
      exec6_[offset] = next_ofs - offset;
      offset = next_ofs;
    }
    exec6_[offset] = exec6_offset_ - (exec7_offset_ + 4 + offset);
  }
#if 0
  for (const auto &exec6 : exec6_) {
    std::cout << exec6.first << ',' << exec6.second << std::endl;
  }
#endif
  return true;
}

bool Exec::ReadExec7() {
  if (exec7_.empty() == false) return true;
  if (exec6_.empty()) {
    if (ReadExec6() == false) return false;
  }
  for (const auto &exec6 : exec6_) {
    p_file_->Seek(exec7_offset_ + 4 + exec6.first, SEEK_SET);
    std::vector<char16_t> buf(exec6.second / 2);
    p_file_->Read(exec6.second, &buf[0]);
    while (buf.back() == u'\0') { buf.pop_back(); }
    exec7_.push_back(std::u16string(&buf[0], buf.size()));
  }
  return true;
}

std::vector<Exec::Exec1>::iterator Exec::FindVariable(int offset) {
  auto it = exec1_.begin();
  for (; it != exec1_.end(); ++it) {
    if (it->type == Exec1::Type::kVariable && it->shift == offset) {
      return it;
    }
  }
  return it;
}

std::vector<Exec::Exec1>::const_iterator Exec::FindVariable(int offset) const {
  return const_cast<Exec*>(this)->FindVariable(offset);
}

int Exec::GetVariableOffset(const std::string& name) const {
  auto it = exec1_.begin();
  for (; it != exec1_.end(); ++it) {
    if (it->type == Exec1::Type::kVariable && UTF16ToUTF8(it->name) == name) {
      return it->shift;
    }
  }
  return -1;
}

std::string Exec::GetVariableName(int offset) const {
  auto it = FindVariable(offset);
  if (it == exec1_.end()) return std::string();
  return UTF16ToUTF8(it->name);
}

int Exec::GetVariableValue(int offset) const {
  auto it = FindVariable(offset);
  if (it == exec1_.end()) return 0;
  return it->value;
}

void Exec::SetVariableValue(int offset, int value) {
  auto it = FindVariable(offset);
  if (it == exec1_.end()) return;
  it->value = value;
}

void Exec::ResetLocalVariableValues() {
  for (auto& v : exec1_) {
    if (v.type == Exec1::Type::kVariable && v.scope != 3) {
      v.value = v.init_value;
    }
  }
}

#if 0
int Exec::GetFunctionOffset(const std::u16string& func_name) const {
  for (const auto& it : exec2_) {
    if (it.func_name == func_name) {
      return it.code_offset;
    }
  }
  return -1;
}

int Exec::GetFunctionOffset(int id) const {
  for (const auto& it : exec2_) {
    if (it.id == id) {
      return it.code_offset;
    }
  }
  return -1;
}

std::u16string Exec::GetFunctionName(int id) const {
  for (const auto& it : exec2_) {
    if (it.id == id) {
      return it.func_name;
    }
  }
  return std::u16string();
}

int Exec::GetFunctionId(const std::u16string& func_name) const {
  for (const auto& it : exec2_) {
    if (it.func_name == func_name) {
      return it.id;
    }
  }
  return -1;
}
#endif

int Exec::GetExternalFuncId(const std::u16string& name) const {
  for (const auto& f : ext_funcs_) {
    if (f.second.name == name) {
      return f.first;
    }
  }
  return -1;
}

std::u16string Exec::GetExternalFuncName(int id) const {
  auto it = ext_funcs_.find(id);
  if (it != ext_funcs_.end()) {
    return it->second.name;
  }
  return std::u16string();
}

int Exec::GetExternalFuncOffset(int id) const {
  auto it = ext_funcs_.find(id);
  if (it != ext_funcs_.end()) {
    return it->second.offset;
  }
  return -1;
}

int Exec::GetExternalFuncOffset(const std::u16string& name) const {
  for (const auto& f : ext_funcs_) {
    if (f.second.name == name) {
      return f.second.offset;
    }
  }
  return -1;
}


int Exec::GetInternalFuncId(const std::u16string& name) const {
  for (const auto& f : int_funcs_) {
    if (f.second.name == name) {
      return f.first;
    }
  }
  return -1;
}

std::u16string Exec::GetInternalFuncName(int id) const {
  auto it = int_funcs_.find(id);
  if (it != int_funcs_.end()) {
    return it->second.name;
  }
  return std::u16string();
}

int Exec::GetInternalFuncOffset(int id) const {
  auto it = int_funcs_.find(id);
  if (it != int_funcs_.end()) {
    return it->second.offset;
  }
  return -1;
}

int Exec::GetInternalFuncOffset(const std::u16string& name) const {
  for (const auto& f : int_funcs_) {
    if (f.second.name == name) {
      return f.second.offset;
    }
  }
  return -1;
}


int Exec::GetLabelOffset(const std::u16string& label_name) const {
  for (const auto& l : labels_) {
    if (l.second == label_name) {
      return l.first;
    }
  }
  return -1;
}

bool Exec::IsLabelOffset(int offset) const {
  for (const auto& l : labels_) {
    if (l.first == offset) {
      return true;
    }
  }
  return false;
}

std::u16string Exec::GetLabelName(int offset) const {
  for (const auto& l : labels_) {
    if (l.first == offset) {
      return l.second;
    }
  }
  return std::u16string();
}


bool Exec::IsTempLabelOffset(int offset) const {
  for (const auto& l : tmp_lbls_) {
    if (l.first == offset) {
      return true;
    }
  }
  return false;

}

std::u16string Exec::GetTempLabelName(int offset) const {
  for (const auto& l : tmp_lbls_) {
    if (l.first == offset) {
      return l.second;
    }
  }
  return std::u16string();
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
        FlushTerminal(list, &text_buf);
        list.push_back(new ExecTextWaitingForKeyIn());
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

ExecTextExpression *Exec::ParseText(int index) {
  ReadExec7();
  if (static_cast<size_t>(index) >= exec7_.size()) {
    return nullptr;
  }
  ExecTextExpression *exp = ParseText(exec7_.at(index));
  exp->DotTest();
  return exp;
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

bool Exec::OutputVariableList(const std::string& file_name) {
  if (exec1_length_ == 0) {
    if (ReadExec1() == false) return false;
  }
  std::ofstream ofs(file_name);
  if (ofs.is_open() == false) return false;
  ofs << "NAME\tTYPE\tSIZE\tSCOPE\tSHIFT\n";
  for (const auto& v : exec1_) {
    std::string type_name;
    switch (v.type) {
    case Exec1::Type::kVariable:
      type_name = "Variable";
      break;
    case Exec1::Type::kString:
      type_name = "String";
      break;
    case Exec1::Type::kArray:
      type_name = "Array";
      break;
    default:
      continue;
    }
    ofs << UTF16ToUTF8(v.name) << '\t' << type_name << '\t'
        << v.size << '\t' << v.scope << '\t'
        << "0x" << std::hex << std::uppercase
        << v.shift << std::dec << std::nouppercase << '\n';
  }
  ofs.close();
  return true;
}

bool Exec::OutputFunctionList(const std::string& file_name) {
  if (exec2_length_ == 0) {
    if (ReadExec2() == false) return false;
  }
  std::ofstream ofs(file_name);
  if (ofs.is_open() == false) return false;
  ofs << "NAME\tID\tIN_LABEL\tOFFSET\n";
  for (const auto& f : exec2_) {
    ofs << UTF16ToUTF8(f.func_name) << '\t' << f.id
        << '\t' << (f.in_label_block ? "TRUE" : "FALSE")
        << '\t' << f.code_offset << '\n';
  }
  ofs.close();
  return true;
}

bool Exec::OutputLabelList(const std::string& file_name) {
  if (exec3_length_ == 0) {
    if (ReadExec3() == false) return false;
  }
  std::ofstream ofs(file_name);
  if (ofs.is_open() == false) return false;
  ofs << "NAME\tOFFSET\n";
  for (const auto& l : exec3_) {
    ofs << UTF16ToUTF8(l.second) << '\t' << l.first << '\n';
  }
  ofs.close();
  return true;
}

} // namespace mlib
