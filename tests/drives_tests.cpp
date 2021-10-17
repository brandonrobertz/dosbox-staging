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

std::string run_Set_Label(char const * const input, bool cdrom) {
    char output[32] = { 0 };
    Set_Label(input, output, cdrom);
    std::cout << "Set_Label " << "CD-ROM? " << (cdrom ? 'y' : 'n') << \
        " Input: " << input << " Output: " << output << '\n';
    return std::string(output);
}

std::string run_Set_Label_STD(const std::string input, bool cdrom) {
    std::string output;
    Set_Label(input, output, cdrom);
    std::cout << "Set_Label (STD) " << "CD-ROM? " << (cdrom ? 'y' : 'n') << \
        " Input: " << input << " Output: " << output << '\n';
    return output;
}

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

/**
 * Set_Labels tests. These test the conversion of a FAT/CD-ROM volume
 * label to an MS-DOS 8.3 label with a variety of edge cases & oddities.
 */
TEST(Set_Label, Daggerfall)
{
    std::string output = run_Set_Label("Daggerfall", false);
    EXPECT_EQ("DAGGERFA.LL", output);
}
TEST(Set_Label, DaggerfallCD)
{
    std::string output = run_Set_Label("Daggerfall", true);
    EXPECT_EQ("Daggerfa.ll", output);
}

TEST(Set_Label, LongerThan11)
{
    std::string output = run_Set_Label("a123456789AAA", false);
    EXPECT_EQ("A1234567.89A", output);
}
TEST(Set_Label, LongerThan11CD)
{
    std::string output = run_Set_Label("a123456789AAA", true);
    EXPECT_EQ("a1234567.89A", output);
}

TEST(Set_Label, ShorterThan8)
{
    std::string output = run_Set_Label("a123456", false);
    EXPECT_EQ("A123456", output);
}
TEST(Set_Label, ShorterThan8CD)
{
    std::string output = run_Set_Label("a123456", true);
    EXPECT_EQ("a123456", output);
}

// Tests that the CD-ROM version adds a trailing dot when
// input is 8 chars plus one dot (9 chars total)
TEST(Set_Label, EqualTo8)
{
    std::string output = run_Set_Label("a1234567", false);
    EXPECT_EQ("A1234567", output);
}
TEST(Set_Label, EqualTo8CD)
{
    std::string output = run_Set_Label("a1234567", true);
    EXPECT_EQ("a1234567.", output);
}

// A test to ensure non-CD-ROM function strips trailing dot
TEST(Set_Label, StripEndingDot)
{
    std::string output = run_Set_Label("a1234567.", false);
    EXPECT_EQ("A1234567", output);
}
TEST(Set_Label, NoStripEndingDotCD)
{
    std::string output = run_Set_Label("a1234567.", true);
    EXPECT_EQ("a1234567.", output);
}

// Just to make sure this function doesn't clean invalid DOS labels
TEST(Set_Label, InvalidCharsEndingDot)
{
    std::string output = run_Set_Label("?*':&@(..", false);
    EXPECT_EQ("?*':&@(.", output);
}
TEST(Set_Label, InvalidCharsEndingDotCD)
{
    std::string output = run_Set_Label("?*':&@(..", true);
    EXPECT_EQ("?*':&@(..", output);
}

/////////////////////////////////////////////////
// NEW CODE TESTS
/////////////////////////////////////////////////
TEST(Set_Label_STD, Daggerfall)
{
    std::string output = run_Set_Label_STD("Daggerfall", false);
    EXPECT_EQ("DAGGERFA.LL",output);
}
TEST(Set_Label_STD, DaggerfallCD)
{
    std::string output = run_Set_Label_STD("Daggerfall", true);
    EXPECT_EQ("Daggerfa.ll",output);
}

TEST(Set_Label_STD, LongerThan11)
{
    std::string output = run_Set_Label_STD("a123456789AAA", false);
    EXPECT_EQ("A1234567.89A",output);
}
TEST(Set_Label_STD, LongerThan11CD)
{
    std::string output = run_Set_Label_STD("a123456789AAA", true);
    EXPECT_EQ("a1234567.89A",output);
}

TEST(Set_Label_STD, ShorterThan8)
{
    std::string output = run_Set_Label_STD("a123456", false);
    EXPECT_EQ("A123456",output);
}
TEST(Set_Label_STD, ShorterThan8CD)
{
    std::string output = run_Set_Label_STD("a123456", true);
    EXPECT_EQ("a123456",output);
}

// Tests that the CD-ROM version adds a trailing dot when
// input is 8 chars plus one dot (9 chars total)
TEST(Set_Label_STD, EqualTo8)
{
    std::string output = run_Set_Label_STD("a1234567", false);
    EXPECT_EQ("A1234567",output);
}
TEST(Set_Label_STD, EqualTo8CD)
{
    std::string output = run_Set_Label_STD("a1234567", true);
    EXPECT_EQ("a1234567.", output);
}
// A test to ensure non-CD-ROM function strips trailing dot
TEST(Set_Label_STD, StripEndingDot)
{
    std::string output = run_Set_Label_STD("a1234567.", false);
    EXPECT_EQ("A1234567", output);
}
TEST(Set_Label_STD, NoStripEndingDotCD)
{
    std::string output = run_Set_Label_STD("a1234567.", true);
    EXPECT_EQ("a1234567.",output);
}

// Just to make sure this function doesn't clean invalid DOS labels
TEST(Set_Label_STD, InvalidCharsEndingDot)
{
    std::string output = run_Set_Label_STD("?*':&@(..", false);
    EXPECT_EQ("?*':&@(.",output);
}
TEST(Set_Label_STD, InvalidCharsEndingDotCD)
{
    std::string output = run_Set_Label_STD("?*':&@(..", true);
    EXPECT_EQ("?*':&@(..",output);
}

} // namespace
