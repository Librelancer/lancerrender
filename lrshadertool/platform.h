#ifndef _PLATFORM_H_
#define _PLATFORM_H_
#include <string>
std::string ReadAllText(std::string path);

#ifdef WIN32
char* win32_realpath(const char* inpath, char* mustNull);
#define realpath win32_realpath
#endif
#endif