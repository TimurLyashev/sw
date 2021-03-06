// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "driver.h"

#include <algorithm>
#include <fstream>
#include <sstream>

int ll_bazellex_init(void **scanner);
int ll_bazellex_destroy(void *yyscanner);
struct yy_buffer_state *ll_bazel_scan_string(const char *yy_str, void *yyscanner);
yy_bazel::parser::symbol_type ll_bazellex(void *yyscanner, yy_bazel::location &loc);

BazelParserDriver::BazelParserDriver()
{
}

yy_bazel::parser::symbol_type BazelParserDriver::lex()
{
    auto ret = ll_bazellex(scanner, location);
    return ret;
}

int BazelParserDriver::parse(const std::string &s)
{
    parseMode = Mode::String;

    ll_bazellex_init(&scanner);
    ll_bazel_scan_string(s.c_str(), scanner);
    auto res = parse();
    ll_bazellex_destroy(scanner);

    return res;
}

int BazelParserDriver::parse()
{
    yy_bazel::parser parser(*this);
    parser.set_debug_level(debug);
    int res = parser.parse();
    return res;
}

void BazelParserDriver::error(const yy_bazel::location &l, const std::string &m)
{
    std::ostringstream ss;
    ss << l << " " << m << "\n";
    if (!can_throw)
        std::cerr << ss.str();
    else
        throw std::runtime_error("Error during bazel parse: " + ss.str());
}

void BazelParserDriver::error(const std::string& m)
{
    std::ostringstream ss;
    ss << m << "\n";
    if (!can_throw)
        std::cerr << ss.str();
    else
        throw std::runtime_error("Error during bazel parse: " + ss.str());
}
