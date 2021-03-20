#ifndef _GL2SPV_H_
#define _GL2SPV_H_

#include <vector>
int CompileShader(const char *inputStr, const char *inputName, const char *defines, bool vertex, std::vector<uint>& spv);

void GlslInit();
void GlslDestroy();
#endif