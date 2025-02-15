/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <iostream>
#include <fstream>

#include "toolutil.h"

std::vector<std::string> pvxs::read_lines(const std::string& file) {
    std::ifstream stream(file, std::ios::in);
    if (!stream.good())
        return {};

    std::vector<std::string> ret;
    
    char line[128] = {}; // PVs can't be longer than 61 chars, but we'll read more anyway
    while (!stream.getline(line, sizeof(line)).eof()) {
        ret.push_back(line);
        line[0] = 0;
    }
    return ret;
}