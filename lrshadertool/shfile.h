#ifndef _SHFILE_H_
#define _SHFILE_H_

#include <string>
#include <vector>

class ShFile {
    public:
        ShFile(const char *instr);
        std::string vertex_source;
        std::string fragment_source;
        std::vector<std::string> features;
};

#endif