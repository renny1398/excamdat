#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <cmath>

#include <stdio.h>
#include <vorbis/vorbisfile.h>
#include <sys/types.h>
#include <dirent.h>

#include "vcedit.h"

using namespace std;

struct loop_struct {
  uint32_t loop_start;
  uint32_t loop_length;
};

int main(int argc, char** argv)
{
  if (argc < 2) {
    cerr << "Invalid argument." << endl;
    return 1;
  }
  
  // set bgm_loop path
  string bgm_loop_path(argv[0]);
  const size_t found_delim = bgm_loop_path.find_last_of("/\\");
  if (found_delim == string::npos) {
    cerr << "invalid parameter 0 (\"" << argv[0] << "\")" << endl;
    return 1;
  }
  bgm_loop_path.replace(found_delim + 1, string::npos,
                        argv[0][found_delim] == '\\' ? "bgm_loop\\" : "bgm_loop/");

  // open csv
  string csv_filename(bgm_loop_path);
  csv_filename.append(argv[1]);
  csv_filename.append(".csv");
  ifstream csv_stream(csv_filename);
  if (csv_stream.is_open() == false) {
    cerr << "Failed to open bgm_loop csv \"" << csv_filename << "\" (product: " << argv[1] << ')' << endl;
    return 1;
  }
  
  // read csv
  map<string, loop_struct> ogg_map;
  do {
    std::pair<string, loop_struct> ogg_loop;
    string str;
    while (getline(csv_stream, str, '\n')){
      string token;
      istringstream iss(str);
      
      getline(iss, ogg_loop.first, ',');
      getline(iss, token, ',');
      ogg_loop.second.loop_start = round(stof(token));
      getline(iss, token, ',');
      ogg_loop.second.loop_length = round(stof(token)) - ogg_loop.second.loop_start;
      
      // cout << ogg_loop.first << " in " << argv[1] << ".csv:" << endl;
      // cout << " LOOPSTART=" << ogg_loop.second.loop_start << endl;
      // cout << " LOOPLENGTH=" << ogg_loop.second.loop_length << endl;

      ogg_map.insert(ogg_loop);
    }
  } while (false);
  csv_stream.close();
  
  // open a specified directory
  string ogg_path("./");
  if (2 < argc) {
    ogg_path.assign(argv[2]);
    if (ogg_path[ogg_path.length() - 1] != '/' && ogg_path[ogg_path.length() - 1] != '\\') {
      ogg_path.append("/");
    }
  }

  DIR *dir = opendir(ogg_path.c_str());
  if (dir == NULL) {
    cerr << "Failed to open a directory \"" << ogg_path << "\"." << endl;
    return 1;
  }
  
  struct dirent *ent;
  while((ent = readdir(dir)) != NULL) {
    
    string key(ent->d_name);
    size_t period_pos = key.find_last_of(".");
    if (period_pos == string::npos) {
      continue;
    }
    key.erase(period_pos);
    map<string, loop_struct>::const_iterator it = ogg_map.find(key);
    if (it == ogg_map.end()) {
      continue;
    }
    
    string filename(ogg_path);
    filename.append(ent->d_name);

    // open vorbis file
    FILE* fh_in = fopen(filename.c_str(), "rb");
    if (fh_in == NULL) {
      cerr << "Failed to open \"" << filename << "\"." << endl;
      return 1;
    }
    
    vcedit_state* state = vcedit_new_state();
    if (vcedit_open(state, fh_in) < 0) {
      cerr << "Failed to vcedit_open \"" << filename << "\"." << endl;
      fclose(fh_in);
      return 1;
    }

    vorbis_comment *vc = vcedit_comments(state);
    vector<string> old_comments;
    for (char** comments = vc->user_comments; *comments; ++comments) {
      old_comments.push_back(string(*comments));
    }
    
    vorbis_comment_clear(vc);
    vorbis_comment_init(vc);
    
    for (vector<string>::const_iterator it = old_comments.begin(); it != old_comments.end(); ++it) {
      vorbis_comment_add(vc, it->c_str());
    }
    
    string tag_loop_start("LOOPSTART=");
    tag_loop_start.append(to_string(it->second.loop_start));
    string tag_loop_length("LOOPLENGTH=");
    tag_loop_length.append(to_string(it->second.loop_length));
    
    vorbis_comment_add(vc, tag_loop_start.c_str());
    vorbis_comment_add(vc, tag_loop_length.c_str());
    cout << "In \"" << filename << "\":" << endl;
    cout << ' ' << tag_loop_start << endl;
    cout << ' ' << tag_loop_length << endl;
  
    // open output file
    string out_filename(ogg_path);
    out_filename.append(it->first).append(".loop.ogg");
    FILE* fh_out = fopen(out_filename.c_str(), "wb");
    if (fh_out == NULL) {
      cerr << "Failed to open \"" << out_filename << "\"." << endl;
      vcedit_clear(state);
      fclose(fh_in);
      return 1;
    }
    
    // write loop information
    vcedit_write(state, fh_out);
    fclose(fh_out);
    
    vcedit_clear(state);
    fclose(fh_in);
  }
  
  return 0;
}
