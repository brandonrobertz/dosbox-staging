/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* This sample shows how to write a simple unit test for dosbox-staging using
 * Google C++ testing framework.
 *
 * Read Google Test Primer for reference of most available features, macros,
 * and guidance about writing unit tests:
 *
 * https://github.com/google/googletest/blob/master/googletest/docs/primer.md#googletest-primer
 */

/* Include necessary header files; order of headers should be as follows:
 *
 * 1. Header declaring functions/classes being tested
 * 2. <gtest/gtest.h>, which declares the testing framework
 * 3. Additional system headers (if needed)
 * 4. Additional dosbox-staging headers (if needed)
 */

#include "drives.h"

#include <gtest/gtest.h>

#include <string>

// Open anonymous namespace (this is Google Test requirement)

namespace {

TEST(get_extension, BasicFunctionality)
{
    EXPECT_EQ("EXE", get_extension("TEST.EXE"));
    EXPECT_EQ(true, get_extension("TEST").empty());
    EXPECT_EQ("EXE", get_extension(".EXE"));
}
TEST(get_filename, BasicFunctionality)
{
    EXPECT_EQ("TEST", get_filename("TEST.EXE"));
    EXPECT_EQ("TEST", get_filename("TEST"));
    EXPECT_EQ(true, get_filename(".EXE").empty());
}

TEST(WildFileCmp, ExactMatch)
{
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "TEST.EXE"));
    EXPECT_EQ(true, WildFileCmp("TEST", "TEST"));
    EXPECT_EQ(false, WildFileCmp("TEST.EXE", ".EXE"));
    EXPECT_EQ(true, WildFileCmp(".EXE", ".EXE"));
}
TEST(WildFileCmpSTD, ExactMatch)
{
    EXPECT_EQ(true, WildFileCmp(std::string("TEST.EXE"), std::string("TEST.EXE")));
    EXPECT_EQ(true, WildFileCmp(std::string("TEST"),     std::string("TEST")));
    EXPECT_EQ(false, WildFileCmp(std::string("TEST.EXE"), std::string(".EXE")));
    EXPECT_EQ(true, WildFileCmp(std::string(".EXE"),     std::string(".EXE")));
}

TEST(WildFileCmp, WildDotWild)
{
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "*.*"));
    EXPECT_EQ(true, WildFileCmp("TEST", "*.*"));
    EXPECT_EQ(true, WildFileCmp(".EXE", "*.*"));
}
TEST(WildFileCmpSTD, WildDotWild)
{
    EXPECT_EQ(true, WildFileCmp(std::string("TEST.EXE"), std::string("*.*")));
    EXPECT_EQ(true, WildFileCmp(std::string("TEST"), std::string("*.*")));
    EXPECT_EQ(true, WildFileCmp(std::string(".EXE"), std::string("*.*")));
}

TEST(WildFileCmp, WildcardNoExt)
{
    EXPECT_EQ(false, WildFileCmp("TEST.EXE", "*"));
    EXPECT_EQ(false, WildFileCmp(".EXE", "*"));
    EXPECT_EQ(true, WildFileCmp("TEST", "*"));
    EXPECT_EQ(true, WildFileCmp("TEST", "T*"));
    EXPECT_EQ(false, WildFileCmp("TEST", "Z*"));
}
TEST(WildFileCmpSTD, WildcardNoExt)
{
    EXPECT_EQ(false, WildFileCmp(std::string("TEST.EXE"), std::string("*")));
    EXPECT_EQ(false, WildFileCmp(std::string(".EXE"), std::string("*")));
    EXPECT_EQ(true, WildFileCmp(std::string("TEST"), std::string("*")));
    EXPECT_EQ(true, WildFileCmp(std::string("TEST"), std::string("T*")));
    EXPECT_EQ(false, WildFileCmp(std::string("TEST"), std::string("Z*")));
}

TEST(WildFileCmp, QuestionMark)
{
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "?EST.EXE"));
    EXPECT_EQ(true, WildFileCmp("TEST", "?EST"));
    EXPECT_EQ(false, WildFileCmp("TEST", "???Z"));
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "TEST.???"));
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "TEST.?XE"));
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "???T.EXE"));
    EXPECT_EQ(true, WildFileCmp("TEST", "???T.???"));
}
TEST(WildFileCmpSTD, QuestionMark)
{
    EXPECT_EQ(true, WildFileCmp(std::string("TEST.EXE"), std::string("?EST.EXE")));
    EXPECT_EQ(true, WildFileCmp(std::string("TEST"), std::string("?EST")));
    EXPECT_EQ(false, WildFileCmp(std::string("TEST"), std::string("???Z")));
    EXPECT_EQ(true, WildFileCmp(std::string("TEST.EXE"), std::string("TEST.???")));
    EXPECT_EQ(true, WildFileCmp(std::string("TEST.EXE"), std::string("TEST.?XE")));
    EXPECT_EQ(true, WildFileCmp(std::string("TEST.EXE"), std::string("???T.EXE")));
    EXPECT_EQ(true, WildFileCmp(std::string("TEST"), std::string("???T.???")));
}

} // namespace
