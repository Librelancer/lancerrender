#include "shfile.h"
#include <string>
#include <sstream>
#include <regex>
#include "platform.h"
#include <stdlib.h>
#include <stdexcept>

#define INC_REGEX "^\\s*@\\s*include\\s*\\(([^\\)]*)\\)\\s*"

std::string FullPath(std::string file)
{
    char *ptr;
    ptr = realpath(file.c_str(), NULL);
    std::string res = std::string(ptr);
    free(ptr);
    return res;
}

std::string GetDirectory(const std::string& str)
{
    size_t found = str.find_last_of("/\\");
    return str.substr(0, found);
}

#ifdef _WIN32
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

static bool StringStartsWith(const std::string& str, const std::string& prefix)
{
    return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
}

static bool StringEndsWith(const std::string& str, const std::string& suffix)
{
    return str.size() >= suffix.size() && 0 == str.compare(str.size()-suffix.size(), suffix.size(), suffix);
}

static bool StringContains(const std::string& s1, const std::string& s2)
{
    return s1.find(s2) != std::string::npos;
}

std::string PathCombine(std::string a, std::string b)
{
    if(StringEndsWith(a, PATH_SEP) || StringStartsWith(b, PATH_SEP)) {
        return (a + b);
    } else {
        return (a + PATH_SEP + b);
    }
}

static std::string ProcessIncludes(std::string fname, std::string src, std::string dir)
{
    std::istringstream f(src);
    std::ostringstream out;
    std::string line;    
    std::smatch sm;
    std::regex e (INC_REGEX, std::regex::ECMAScript);
    bool inMultilineComment = false;
    while (std::getline(f, line)) {
        if(!inMultilineComment) {
            size_t commentStart = line.find("/*");
            if(commentStart != std::string::npos) {
                if(line.find("*/", commentStart) == std::string::npos) {
                    inMultilineComment = true;
                    goto printline;
                }
            }
            if(regex_match(line, sm, e)) {
                if(sm.size() < 2) {
                    throw std::invalid_argument("include matching in" + fname);
                }
                std::string filearg = sm[1].str();
                std::string infile = PathCombine(dir, filearg);
                std::string insrc = ReadAllText(infile);
                std::string inc = ProcessIncludes(infile, insrc, GetDirectory(infile));
                out << inc << std::endl;
                continue;
            }
        } else {
            if(StringContains(line, "*/")) inMultilineComment = false;
        }
        printline:
        out << line << std::endl;
    }
    return out.str();
}

static std::string trimStart(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    return s;
}

static std::string trimBoth(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
     s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    return s;
}


#define BLOCK_NONE (0)
#define BLOCK_VERTEX (1)
#define BLOCK_FRAGMENT (2)

ShFile::ShFile(const char *infile)
{
    std::string src = ReadAllText(infile);
    std::string path = FullPath(std::string(infile));
    src = ProcessIncludes(FullPath(path), src, GetDirectory(path));
    std::istringstream f(src);
    std::ostringstream cblock;
    std::string line;
    int inBlock = BLOCK_NONE;
    bool hasVertex = false;
    bool hasFragment = false;
    bool inMultilineComment = false;

    while (std::getline(f, line)) {
        if(!inMultilineComment) {
            size_t commentStart = line.find("/*");
            if(commentStart != std::string::npos) {
                if(line.find("*/", commentStart) == std::string::npos) {
                    inMultilineComment = true;
                    goto printline;
                }
            }
            /*directive*/
            std::string trimmed = trimStart(line);
            if(StringStartsWith(trimmed, "@vertex")) {
                if(inBlock) {
                    if(inBlock == BLOCK_VERTEX) this->vertex_source = cblock.str();
                    if(inBlock == BLOCK_FRAGMENT) this->fragment_source = cblock.str();
                    cblock = std::ostringstream();
                }
                cblock << "#define VERTEX_SHADER\n";
                cblock << "#line 1000\n";
                inBlock = BLOCK_VERTEX;
                hasVertex = true;
                continue;
            } else if (StringStartsWith(trimmed, "@fragment")) {
                if(inBlock) {
                    if(inBlock == BLOCK_VERTEX) this->vertex_source = cblock.str();
                    if(inBlock == BLOCK_FRAGMENT) this->fragment_source = cblock.str();
                    cblock = std::ostringstream();
                }
                cblock << "#define FRAGMENT_SHADER\n";
                cblock << "#line 1000\n";
                inBlock = BLOCK_FRAGMENT;
                hasFragment = true;
                continue;
            } else if (StringStartsWith(trimmed, "@feature")) {
                std::string nf = trimBoth(trimmed.substr(8));
                if(nf != "") {
                    this->features.push_back(nf);
                }
                continue;
            }
        } else {
           if(StringContains(line, "*/")) inMultilineComment = false; 
        }
        printline:
        if(inBlock) cblock << line << std::endl;
    }
    if(inBlock) {
        if(inBlock == BLOCK_VERTEX) this->vertex_source = cblock.str();
        if(inBlock == BLOCK_FRAGMENT) this->fragment_source = cblock.str();
    }
    if(!hasVertex) throw std::invalid_argument("shader file does not have vertex shader");
    if(!hasFragment) throw std::invalid_argument("shader file does not have fragment shader");
}