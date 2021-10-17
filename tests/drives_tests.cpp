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

TEST(Set_Label, Daggerfall)
{
    std::string input = "Daggerfall";
    char output[9] = { 0 };
    bool cdrom = false;
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    Set_Label(input.c_str(), output, cdrom);
    EXPECT_EQ("DAGGERFA.LL",std::string(output));
}
TEST(Set_Label, DaggerfallCD)
{
    std::string input = "Daggerfall";
    char output[9] = { 0 };
    bool cdrom = true;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("Daggerfa.ll",std::string(output));
}

TEST(Set_Label, LongerThan11)
{
    std::string input = "a123456789AAA";
    char output[9] = { 0 };
    bool cdrom = false;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("A1234567.89A",std::string(output));
}
TEST(Set_Label, LongerThan11CD)
{
    std::string input = "a123456789AAA";
    char output[9] = { 0 };
    bool cdrom = true;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("a1234567.89A",std::string(output));
}

TEST(Set_Label, ShorterThan8)
{
    std::string input = "a123456";
    char output[9] = { 0 };
    bool cdrom = false;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("A123456",std::string(output));
}
TEST(Set_Label, ShorterThan8CD)
{
    std::string input = "a123456";
    char output[9] = { 0 };
    bool cdrom = true;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("a123456",std::string(output));
}

// Tests that the CD-ROM version adds a trailing dot when
// input is 8 chars plus one dot (9 chars total)
TEST(Set_Label, EqualTo8)
{
    std::string input = "a1234567";
    char output[9] = { 0 };
    bool cdrom = false;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("A1234567",std::string(output));
}
TEST(Set_Label, EqualTo8CD)
{
    std::string input = "a1234567";
    char output[9] = { 0 };
    bool cdrom = true;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("a1234567.",std::string(output));
}

// A test to ensure non-CD-ROM function strips trailing dot
TEST(Set_Label, StripEndingDot)
{
    std::string input = "a1234567.";
    char output[9] = { 0 };
    bool cdrom = false;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("A1234567",std::string(output));
}
TEST(Set_Label, NoStripEndingDotCD)
{
    std::string input = "a1234567.";
    char output[9] = { 0 };
    bool cdrom = true;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("a1234567.",std::string(output));
}

// Just to make sure this function doesn't clean invalid DOS labels
TEST(Set_Label, InvalidCharsEndingDot)
{
    std::string input = "?*':&@(..";
    char output[9] = { 0 };
    bool cdrom = false;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("?*':&@(.",std::string(output));
}
TEST(Set_Label, InvalidCharsEndingDotCD)
{
    std::string input = "?*':&@(..";
    char output[9] = { 0 };
    bool cdrom = true;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("?*':&@(..",std::string(output));
}

/////////////////////////////////////////////////
// NEW CODE TESTS
/////////////////////////////////////////////////
TEST(Set_Label_STD, Daggerfall)
{
    const std::string input = "Daggerfall";
    std::string output;
    bool cdrom = false;
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    Set_Label(input, output, cdrom);
    EXPECT_EQ("DAGGERFA.LL",output);
}
TEST(Set_Label_STD, DaggerfallCD)
{
    const std::string input = "Daggerfall";
    std::string output;
    bool cdrom = true;
    Set_Label(input, output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("Daggerfa.ll",output);
}

TEST(Set_Label_STD, LongerThan11)
{
    const std::string input = "a123456789AAA";
    std::string output;
    bool cdrom = false;
    Set_Label(input, output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("A1234567.89A",output);
}
TEST(Set_Label_STD, LongerThan11CD)
{
    const std::string input = "a123456789AAA";
    std::string output;
    bool cdrom = true;
    Set_Label(input, output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("a1234567.89A",output);
}

TEST(Set_Label_STD, ShorterThan8)
{
    const std::string input = "a123456";
    std::string output;
    bool cdrom = false;
    Set_Label(input, output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("A123456",output);
}
TEST(Set_Label_STD, ShorterThan8CD)
{
    const std::string input = "a123456";
    std::string output;
    bool cdrom = true;
    Set_Label(input, output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("a123456",output);
}

// Tests that the CD-ROM version adds a trailing dot when
// input is 8 chars plus one dot (9 chars total)
TEST(Set_Label_STD, EqualTo8)
{
    const std::string input = "a1234567";
    std::string output;
    bool cdrom = false;
    Set_Label(input, output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("A1234567",output);
}
TEST(Set_Label_STD, EqualTo8CD)
{
    const std::string input = "a1234567";
    std::string output;
    bool cdrom = true;
    Set_Label(input, output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("a1234567.", output);
}
// A test to ensure non-CD-ROM function strips trailing dot
TEST(Set_Label_STD, StripEndingDot)
{
    const std::string input = "a1234567.";
    std::string output;
    bool cdrom = false;
    Set_Label(input, output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("A1234567", output);
}
TEST(Set_Label_STD, NoStripEndingDotCD)
{
    const std::string input = "a1234567.";
    std::string output;
    bool cdrom = true;
    Set_Label(input, output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("a1234567.",output);
}

// Just to make sure this function doesn't clean invalid DOS labels
TEST(Set_Label_STD, InvalidCharsEndingDot)
{
    const std::string input = "?*':&@(..";
    std::string output;
    bool cdrom = false;
    Set_Label(input, output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("?*':&@(.",output);
}
TEST(Set_Label_STD, InvalidCharsEndingDotCD)
{
    const std::string input = "?*':&@(..";
    std::string output;
    bool cdrom = true;
    Set_Label(input, output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ("?*':&@(..",output);
}

} // namespace
