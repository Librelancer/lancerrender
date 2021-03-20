#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include <utility>
#include "gl2spv.h"
#include "spirv_glsl.hpp"
#include "miniz.h"
#include "shfile.h"
#include "platform.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

const char* flattenBlocks[] = {
    "vs_Material",
    "fs_Material"
};

static int DoFlatten(const char *name) {
    for(int i = 0; i < sizeof(flattenBlocks) / sizeof(const char*); i++) {
        if(!strcmp(name, flattenBlocks[i]))
            return 1;
    }
    return 0;
}

int ToGLSL(std::vector<uint>& spirv_binary, std::string& outstr)
{
    try
    {
        spirv_cross::CompilerGLSL glsl(std::move(spirv_binary));
	    spirv_cross::ShaderResources res = glsl.get_shader_resources();
        for(auto &ubo : res.uniform_buffers) {
            if(DoFlatten(ubo.name.c_str())) {
                glsl.flatten_buffer_block(ubo.id);
            }
        }

        spirv_cross::CompilerGLSL::Options options;
        options.version = 150;
        options.es = false;
        options.enable_420pack_extension = false;
        glsl.set_common_options(options);

        std::string source = glsl.compile();
        source.erase(0, source.find("\n") + 1); //remove version directive
        outstr = "#ifdef GL_ES\nprecision highp float;\nprecision highp int;\n#endif\n" + source;
        return 1;
    }
    catch(const std::exception& e)
    {
		fprintf(stderr, "Compiler threw an exception: %s\n", e.what());
		return 0;
    }    
}

static void IntPermute(std::vector<int> flags, std::vector<int>& ret)
{
    std::vector<int> valsinv;
    for (auto x : flags) {
        valsinv.push_back(~x);
    }
    int max = 0;
    for (auto x : flags) max |= x;
    for(int i = 0; i <= max; i++) {
        int unaccountedBits = i;
        for(auto x : valsinv) {
            unaccountedBits &= x;
            if(unaccountedBits == 0) {
                if(i != 0)
                    ret.push_back(i);
                break;
            }
        }
    }
}

struct FeaturePair {
    std::vector<std::string> strings;
    int flags;
};

typedef std::vector<FeaturePair> StringVectorVector;

static StringVectorVector Permute(std::vector<std::string> defs, std::vector<int> flags)
{
    std::vector<int> combos;
    StringVectorVector strcombos;
    IntPermute(flags, combos);
    for (auto c : combos) {
        std::vector<std::string> strings;
        for(int i = 0; i < flags.size(); i++) {
            int flag = flags[i];
            if((c & flag) == flag) {
                strings.push_back(defs[i]);
            }
        }
        FeaturePair pair = {
            .strings = strings,
            .flags = c
        };
        strcombos.push_back(pair);
    }
    return strcombos;
}

std::string GetFilename(std::string filename)
{
    const size_t last_slash_idx = filename.find_last_of("\\/");
    if (std::string::npos != last_slash_idx)
    {
        filename.erase(0, last_slash_idx + 1);
    }
    return filename;
}

struct CompiledShader {
    int flags;
    std::string vertex;
    std::string fragment;
};

int CompilerMain(int argc, char **argv)
{
    char *arg1 = NULL;
    char *arg2 = NULL;
    char *featureList = NULL;

    int processingArgs = 1;
    int showHelp = 0;
    int verbose = 0;
    for(int i = 1; i < argc; i++) {
        if(processingArgs && argv[i][0] == '-') {
            if(argv[i][1] == '-') {
                if(!strcmp(argv[i], "--help")) {
                    showHelp = 1;
                    continue;
                }
                if(!strcmp(argv[i], "--verbose")) {
                    showHelp = 1;
                    continue;
                }
                if(!strcmp(argv[i], "--feature-list")) {
                    if(argc <= (i + 1)) {
                        fprintf(stderr, "option --feature-list requires argument\n");
                        return 1;
                    }
                    featureList = argv[++i];
                    continue;
                }
            } else {
                switch(argv[i][1]) {
                    case '\0': //-
                        goto setarg;
                    case 'h':
                        showHelp = 1;
                        continue;
                    case 'v':
                        verbose = 1;
                        continue;
                    case 'f':
                        if(argc <= (i + 1)) {
                            fprintf(stderr, "option -f requires argument\n");
                            return 1;
                        }
                        featureList = argv[++i];
                        continue;
                }
            }
            if(!strcmp(argv[i], "--")) {
                processingArgs = 0;
                continue;
            }
            fprintf(stderr, "Invalid option %s\n", argv[i]);
            return 1;
        }
        setarg:
        if(arg2) continue;
        if(arg1) { arg2 = argv[i]; }
        else arg1 = argv[i];
    }

    if(showHelp) {
        printf("lrshadertool\n");
        printf("Usage %s shader output\n", argv[0]);
        printf("validates shader file and compiles all possible feature combinations.\n");
        printf("OPTIONS:\n");
        printf("-h|--help:\t\t\t\tShows this message\n");
        printf("-f|--feature-list [file]:\t\tspecify file containing list of features for bitfield use\n");
        return 0;
    }

    if(!arg1 || !arg2) {
        printf("Usage %s [options] shader output\n", argv[0]);
        printf("Try %s --help for more info.\n", argv[0]);
        return 0;
    }
    if(!strcmp(arg1, "-")) {
        fprintf(stderr, "Cannot read shader input from stdin.\n");
        return 2;
    }
    if(!strcmp(arg2, "-")) {
        fprintf(stderr, "Cannot write shader output to stdout.\n");
        return 2;
    }
    if(access(arg1, F_OK) != 0 ) {
        fprintf(stderr, "Input file `%s` does not exist.\n", arg1);
        return 2;
    }
    if (featureList && access (featureList, F_OK) != 0) {
        fprintf(stderr, "FeatureList file '%s' does not exist.\n",featureList);
        return 2;
    }


    ShFile shader(arg1);

    std::string filename = GetFilename(arg1);
    std::vector<uint> spv;
    std::string outGlsl;

    std::vector<std::string> flStrings;
    std::vector<int> flFlags;
    std::vector<int> shFlags;

    if(featureList) {
        if(verbose) {
            fprintf(stderr, "Using feature list for bitflags order.\n");
        }
        std::istringstream flist(ReadAllText(featureList));
        std::string nextLine;
        while(std::getline(flist, nextLine)) {
            for(int i = 0; i < nextLine.size(); i++) {
                if(!isspace(nextLine[i]))
                {
                    flStrings.push_back(nextLine);
                    break;
                }
            }
        }
        for(int i = 0; i < flStrings.size(); i++) {
            flFlags.push_back(1 << i);
        }
        /* sort features */
        for(int i = 0; i < shader.features.size(); i++) {
            auto pos = std::find(flStrings.begin(), flStrings.end(), shader.features[i]);
            if(pos == flStrings.end()) {
                fprintf(stderr, "Feature `%s` in file `%s` was not present in feature list\n", shader.features[i].c_str(), arg1);
                return 1;
            }
            int index = std::distance( flStrings.begin(), pos );
            if(verbose) {
                fprintf(stderr, "Feature `%s`: flag 0x%x\n", shader.features[i].c_str(), flFlags[index]);
            }
            shFlags.push_back(flFlags[index]);
        }
    } else {
        for(int i = 0; i < shader.features.size(); i++) {
            shFlags.push_back(1 << i);
        }
    }

    //Compile
    if(verbose) fprintf(stderr, "Compiling without flags\n");
    std::vector<CompiledShader> compiled;
    StringVectorVector permutations = Permute(shader.features, shFlags);
    {
        CompiledShader zero;
        zero.flags = 0;
        //sort vertex shader
        if(!CompileShader(shader.vertex_source.c_str(), filename.c_str(), "", true, spv)) {
            fprintf(stderr, "vertex shader compilation failed\n");
            return 1;
        }
        if(!ToGLSL(spv, outGlsl)) {
            fprintf(stderr, "vertex shader compilation failed\n");
            return 1;
        }
        zero.vertex = outGlsl;
        spv = std::vector<uint>();
        if(!CompileShader(shader.fragment_source.c_str(), filename.c_str(), "", false, spv)) {
            fprintf(stderr, "fragment shader compilation failed\n");
            return 1;
        }
        if(!ToGLSL(spv, outGlsl)) {
            fprintf(stderr, "fragment shader compilation failed\n");
            return 1;
        }
        zero.fragment = outGlsl;
        compiled.push_back(zero);
    }

    for(auto& x: permutations) {
        //Print current flags
        if(verbose) {
            fprintf(stderr, "Compiling with 0x%x: ", x.flags);
            int a = 0;
            for(auto& s: x.strings) {
                if(a) fprintf(stderr, ", ");
                a = 1;
                fprintf(stderr, "%s", s.c_str());
            }
            fprintf(stderr, "\n");
        }
        //Compiling
        CompiledShader sh;
        sh.flags = x.flags;
        std::ostringstream defineBlock;
        for(auto& s: x.strings) {
            defineBlock << "#define " << s << "\n";
        }
        std::string defs = defineBlock.str();
        const char *chars_def = defs.c_str();
        spv = std::vector<uint>();
        if(!CompileShader(shader.vertex_source.c_str(), filename.c_str(), chars_def, true, spv)) {
            fprintf(stderr, "vertex shader compilation failed\n using %s\n", chars_def);
            return 1;
        }
        if(!ToGLSL(spv, outGlsl)) {
            fprintf(stderr, "vertex shader compilation failed\nusing %s\n", chars_def);
            return 1;
        }
        spv = std::vector<uint>();
        sh.vertex = outGlsl;
         if(!CompileShader(shader.fragment_source.c_str(), filename.c_str(), chars_def, false, spv)) {
            fprintf(stderr, "fragment shader compilation failed\nusing %s\n", chars_def);
            return 1;
        }
        if(!ToGLSL(spv, outGlsl)) {
            fprintf(stderr, "fragment shader compilation failed\nusing %s\n", chars_def);
            return 1;
        }
        sh.fragment = outGlsl;
        compiled.push_back(sh);
    }
    //Write to file
    /*std::ofstream outfile(arg2);
    for (auto &x : compiled) {
        outfile << "++flags: " << x.flags << "\n";
        outfile << "== VERTEX ==\n";
        outfile << x.vertex << "\n";
        outfile << "== FRAGMENT ==\n";
        outfile << x.fragment << "\n";
    }*/

    int size = sizeof(uint32_t);
    for(auto &x : compiled) {
        size += sizeof(uint32_t); //flags
        size += sizeof(uint32_t); //vertLength
        size += x.vertex.size(); //vertSize (plus trailing zero)
        size += sizeof(uint32_t); //fragLength
        size += x.fragment.size(); //fragSize (plus trailing zero)
    }

    unsigned char *data = (unsigned char*)malloc(size);
    unsigned char *dptr = data;

    #define WRITE_INT(x) do { \
        *(uint32_t*)(dptr) = (uint32_t)(x);\
        dptr += 4;\
    } while(0)
    WRITE_INT(compiled.size());
    for(auto &x : compiled) {
        WRITE_INT(x.flags);
        WRITE_INT(x.vertex.size());
        memcpy((void*)dptr, x.vertex.c_str(), x.vertex.size());
        dptr += x.vertex.size();
        WRITE_INT(x.fragment.size());
        memcpy((void*)dptr, x.fragment.c_str(), x.fragment.size());
        dptr += x.fragment.size();
    }

    unsigned char *comp = (unsigned char*)malloc(compressBound(size));
    mz_ulong compSize = compressBound(size);
    int cmp_status = compress(comp, &compSize, data, size);
    free(data);
    if (cmp_status != Z_OK)
    {
        fprintf(stderr, "compress() failed!\n");
        return 1;
    }

    FILE *out = fopen(arg2, "wb");
    if(!out) {
        fprintf(stderr, "could not open file for writing %s", arg2);
        return 1;
    }
    uint32_t sz_dat = (uint32_t)compSize;
    uint32_t magic = 0xABCDABCD;
    fwrite(&magic, 4, 1, out);
    fwrite(&sz_dat, 4, 1, out);
    fwrite(&size, 4, 1, out);
    fwrite(comp, compSize, 1, out);
    fclose(out);
    return 0;
}




int main(int argc, char **argv)
{
    int retval = 0;
    GlslInit();
    try
    {
        retval = CompilerMain(argc, argv);
    }
    catch(const std::exception& e)
    {
        fprintf(stderr, "%s\n", e.what());
        retval = 1;
    }
    GlslDestroy();
    return retval;
}

