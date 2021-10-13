/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include "drives.h"
#include "string_utils.h"

#include <cctype>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>


std::string_view get_extension(const std::string& name) {
    auto pos = name.find_last_of('.');
    if (pos == std::string::npos) {
        return std::string_view{""};
    }
    return std::string_view{ &name.at(pos)+1 };
}

std::string_view get_filename(const std::string& name) {
    std::string_view name_v{name};
    auto pos = name_v.find_last_of('.');
    if (pos == std::string::npos) {
        return name_v;
    }
    return name_v.substr(0, pos);
}

/**
 * Compare a string with another string that optionally contains
 * wildcard characters asterisk and question mark.
 */
bool wild_equals(const std::string_view &name, const std::string_view &wild, const std::string::size_type size) {
    // both blank, match
    if (name.empty() && wild.empty()) return true;
    // iterate on a character-by-character basis so we can only
    // pass over the strings once. otherwise we can use std::compare or
    // regexes if we don't care about performance here
    const auto wild_size = wild.size();
    const auto name_size = name.size();
    for (std::string::size_type i = 0; i < size; i++) {
        auto w_chr = i < wild_size ? toupper(wild[i]) : ' ';
        auto f_chr = i < name_size ? toupper(name[i]) : ' ';
        // break here and continue with check if we have a wildcard
        if (w_chr == '*') return true;
        // single char wildcard, don't bother comparing
        if (w_chr == '?') continue;
        // we have non-wildcard, compare
        if (w_chr != f_chr)  return false;
    }
    // if we're here, we don't have wildcards and we've passed all
    // matches up to wild length. final check makes sure that
    // the two strings are of equal length
    return true;
}

/**
 * Compare an 8.3 filename with another 8.3 filename, optionally
 * containing wildcards * and ?.
 * NOTE: This contains some oddities that exist within DOSBox, but not
 * actual MSDOS:
 *
 *   - `*` will match `TEST` but not `TEST.EXE`, in MSDOS `*` functions
 *     the same as `*.*` (e.g., `DIR *` in MSDOS 6.22)
 *   - This function treats shorter than 8/3 strings space padded, but
 *     MSDOS appears to treat missing as a wildcard? Check this!
 */
bool WildFileCmp(const std::string &file, const std::string &wild)
{
    const auto wild_name = get_filename(wild);
    const auto file_name = get_filename(file);

    const auto wild_ext = get_extension(wild);
    const auto file_ext = get_extension(file);

    if (!wild_equals(file_name, wild_name, 8)) return false;
    if (!wild_equals(file_ext, wild_ext, 3)) return false;

    return true;
}

bool WildFileCmp(const char *file, const char *wild)
{
	char file_name[9];
	char file_ext[4];
	char wild_name[9];
	char wild_ext[4];
	const char * find_ext;
    // a generic pointer used for comparison
	Bitu r;

	strcpy(file_name,"        ");
	strcpy(file_ext,"   ");
	strcpy(wild_name,"        ");
	strcpy(wild_ext,"   ");

    // find position of the dot in the filename
	find_ext=strrchr(file,'.');
	if (find_ext) {
        // compute length of filename w/o extension, max 8
		Bitu size=(Bitu)(find_ext-file);
		if (size>8) size=8;
        // copy the filename w/o extension
		memcpy(file_name,file,size);
        // skip the dot
		find_ext++;
        // copy the extension
		memcpy(file_ext, find_ext, strnlen(find_ext, 3));
	} else {
        // no extension, copy up to 8 chars as filename
		memcpy(file_name, file, strnlen(file, 8));
	}
    // all filenames are uppercase
	upcase(file_name);upcase(file_ext);

    // Duplicated code from above but on the search pattern
	find_ext=strrchr(wild,'.');
	if (find_ext) {
		Bitu size=(Bitu)(find_ext-wild);
		if (size>8) size=8;
		memcpy(wild_name,wild,size);
		find_ext++;
		memcpy(wild_ext, find_ext, strnlen(find_ext, 3));
	} else {
		memcpy(wild_name, wild, strnlen(wild, 8));
	}
	upcase(wild_name);upcase(wild_ext);

	/* Names are right do some checking */
	r=0;
	while (r<8) {
		if (wild_name[r]=='*') goto checkext;
		if (wild_name[r]!='?' && wild_name[r]!=file_name[r]) return false;
		r++;
	}
checkext:
    r=0;
	while (r<3) {
		if (wild_ext[r]=='*') return true;
		if (wild_ext[r]!='?' && wild_ext[r]!=file_ext[r]) return false;
		r++;
	}
	return true;
}

// TODO Right now label formatting seems to be a bit of mess, with various
// places in code setting/expecting different format, so simple GetLabel() on
// a drive object might not yield an expected result. Not sure how to sort it
// out, but it will require some attention to detail.
// Also: this function is too strict - it removes all punctuation when *some*
// punctuation is acceptable in drive labels (e.g. '_' or '-').
//
std::string To_Label(const char* name) {
	// Reformat the name per the DOS label specification:
	// - Upper-case, up to 11 ASCII characters
	// - Internal spaces allowed but no: tabs ? / \ | . , ; : + = [ ] < > " '
	std::string label(name);
	trim(label); // strip front-and-back white-space
	strip_punctuation(label); // strip all punctuation
	label.resize(11); // collapse remainder to (at-most) 11 chars
	upcase(label);
	return label;
}

void Set_Label(char const * const input, char * const output, bool cdrom) {
	Bitu togo     = 8;
	Bitu vnamePos = 0;
	Bitu labelPos = 0;
	bool point    = false;

	//spacepadding the filenamepart to include spaces after the terminating zero is more closely to the specs. (not doing this now)
	// HELLO\0' '' '

	while (togo > 0) {
		if (input[vnamePos]==0) break;
		if (!point && (input[vnamePos]=='.')) {	togo=4; point=true; }

		//another mscdex quirk. Label is not always uppercase. (Daggerfall)
		output[labelPos] = (cdrom?input[vnamePos]:toupper(input[vnamePos]));

		labelPos++; vnamePos++;
		togo--;
		if ((togo==0) && !point) {
			if (input[vnamePos]=='.') vnamePos++;
			output[labelPos]='.'; labelPos++; point=true; togo=3;
		}
	};
	output[labelPos]=0;

	//Remove trailing dot. except when on cdrom and filename is exactly 8 (9 including the dot) letters. MSCDEX feature/bug (fifa96 cdrom detection)
	if((labelPos > 0) && (output[labelPos-1] == '.') && !(cdrom && labelPos ==9))
		output[labelPos-1] = 0;
}

DOS_Drive::DOS_Drive()
	: dirCache()
{
	curdir[0] = '\0';
	info[0] = '\0';
}

void DOS_Drive::SetDir(const char *path)
{
	safe_strcpy(curdir, path);
}

// static members variables
int DriveManager::currentDrive;
DriveManager::DriveInfo DriveManager::driveInfos[26];

void DriveManager::AppendDisk(int drive, DOS_Drive* disk) {
	driveInfos[drive].disks.push_back(disk);
}

void DriveManager::InitializeDrive(int drive) {
	currentDrive = drive;
	DriveInfo& driveInfo = driveInfos[currentDrive];
	if (driveInfo.disks.size() > 0) {
		driveInfo.currentDisk = 0;
		DOS_Drive* disk = driveInfo.disks[driveInfo.currentDisk];
		Drives[currentDrive] = disk;
		if (driveInfo.disks.size() > 1) disk->Activate();
	}
}

/*
void DriveManager::CycleDrive(bool pressed) {
	if (!pressed) return;
		
	// do one round through all drives or stop at the next drive with multiple disks
	int oldDrive = currentDrive;
	do {
		currentDrive = (currentDrive + 1) % DOS_DRIVES;
		int numDisks = driveInfos[currentDrive].disks.size();
		if (numDisks > 1) break;
	} while (currentDrive != oldDrive);
}

void DriveManager::CycleDisk(bool pressed) {
	if (!pressed) return;
	
	int numDisks = driveInfos[currentDrive].disks.size();
	if (numDisks > 1) {
		// cycle disk
		int currentDisk = driveInfos[currentDrive].currentDisk;
		DOS_Drive* oldDisk = driveInfos[currentDrive].disks[currentDisk];
		currentDisk = (currentDisk + 1) % numDisks;		
		DOS_Drive* newDisk = driveInfos[currentDrive].disks[currentDisk];
		driveInfos[currentDrive].currentDisk = currentDisk;
		
		// copy working directory, acquire system resources and finally switch to next drive		
		strcpy(newDisk->curdir, oldDisk->curdir);
		newDisk->Activate();
		Drives[currentDrive] = newDisk;
	}
}
*/

void DriveManager::CycleDisks(int drive, bool notify) {
	int numDisks = (int)driveInfos[drive].disks.size();
	if (numDisks > 1) {
		// cycle disk
		int currentDisk = driveInfos[drive].currentDisk;
		DOS_Drive* oldDisk = driveInfos[drive].disks[currentDisk];
		currentDisk = (currentDisk + 1) % numDisks;		
		DOS_Drive* newDisk = driveInfos[drive].disks[currentDisk];
		driveInfos[drive].currentDisk = currentDisk;
		
		// copy working directory, acquire system resources and finally switch to next drive		
		strcpy(newDisk->curdir, oldDisk->curdir);
		newDisk->Activate();
		Drives[drive] = newDisk;
		if (notify) LOG_MSG("Drive %c: disk %d of %d now active", 'A'+drive, currentDisk+1, numDisks);
	}
}

void DriveManager::CycleAllDisks(void) {
	for (int idrive=0; idrive<DOS_DRIVES; idrive++) CycleDisks(idrive, true);
}

int DriveManager::UnmountDrive(int drive) {
	int result = 0;
	// unmanaged drive
	if (driveInfos[drive].disks.size() == 0) {
		result = Drives[drive]->UnMount();
	} else {
		// managed drive
		int currentDisk = driveInfos[drive].currentDisk;
		result = driveInfos[drive].disks[currentDisk]->UnMount();
		// only delete on success, current disk set to NULL because of UnMount
		if (result == 0) {
			driveInfos[drive].disks[currentDisk] = NULL;
			for (int i = 0; i < (int)driveInfos[drive].disks.size(); i++) {
				delete driveInfos[drive].disks[i];
			}
			driveInfos[drive].disks.clear();
		}
	}
	
	return result;
}

void DriveManager::Init(Section* /* sec */) {
	
	// setup driveInfos structure
	currentDrive = 0;
	for(int i = 0; i < DOS_DRIVES; i++) {
		driveInfos[i].currentDisk = 0;
	}

	// MAPPER_AddHandler(&CycleDisk, SDL_SCANCODE_F3, MMOD1,
	//                   "cycledisk", "Cycle Disk");
	// MAPPER_AddHandler(&CycleDrive, SDL_SCANCODE_F3, MMOD2,
	//                   "cycledrive", "Cycle Drv");
}

void DRIVES_Init(Section* sec) {
	DriveManager::Init(sec);
}
