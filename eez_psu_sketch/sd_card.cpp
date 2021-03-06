/*
 * EEZ PSU Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "psu.h"

#if OPTION_SD_CARD

#include "sd_card.h"
#include "datetime.h"

#include "list.h"
#include "profile.h"

#if OPTION_DISPLAY
#include "gui.h"
#endif

SdFat SD;

namespace eez {
namespace psu {
namespace sd_card {

TestResult g_testResult = TEST_FAILED;

////////////////////////////////////////////////////////////////////////////////

void dateTime(uint16_t* date, uint16_t* time) {
    int year, month, day, hour, minute, second;
    datetime::breakTime(datetime::nowUtc(), year, month, day, hour, minute, second);
    *date = FAT_DATE(year, month, day);
    *time = FAT_TIME(hour, minute, second);
}

void init() {
    if (!SD.begin(LCDSD_CS, SPI_DIV3_SPEED)) {
        g_testResult = TEST_FAILED;
    } else {
#ifdef EEZ_PSU_SIMULATOR
        makeParentDir("/");
#endif
        g_testResult = TEST_OK;

        SdFile::dateTimeCallback(dateTime);
    }
}

bool test() {
    return g_testResult != TEST_FAILED;
}

#ifndef isSpace
bool isSpace(int c) {
    return c == '\r' || c == '\n' || c == '\t' || c == ' ';
}
#endif

void matchZeroOrMoreSpaces(File &file) {
    while (true) {
        int c = file.peek();
        if (!isSpace(c)) {
            return;
        }
        file.read();
    }
}

bool match(File& file, char c) {
    matchZeroOrMoreSpaces(file);
    if (file.peek() == c) {
        file.read();
        return true;
    }
    return false;
}

bool match(File& file, float &result) {
    matchZeroOrMoreSpaces(file);

    int c = file.peek();
    if (c == -1) {
        return false;
    }

    bool isNegative;
    if (c == '-') {
        file.read();
        isNegative = true;
        c = file.peek();
    } else {
        isNegative = false;
    }

    bool isFraction = false;
    float fraction = 1.0;

    long value = -1;

    while (true) {
        if (c == '.') {
            if (isFraction) {
                return false;
            }
            isFraction = true;
        } else if (c >= '0' && c <= '9') {
            if (value == -1) {
                value = 0;
            }

            value = value * 10 + c - '0';

            if (isFraction) {
                fraction *= 0.1f;
            }
        } else {
            if (value == -1) {
                return false;
            }

            result = (float)value;
            if (isNegative) {
                result = -result;
            }
            if (isFraction) {
                result *= fraction;
            }

            return true;
        }

        file.read();
        c = file.peek();
   }
}

bool makeParentDir(const char *filePath) {
    char dirPath[MAX_PATH_LENGTH];
    util::getParentDir(filePath, dirPath);
    return SD.mkdir(dirPath);
}

bool exists(const char *dirPath, int *err) {
    if (sd_card::g_testResult != TEST_OK) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    if (!SD.exists(dirPath)) {
        if (err) *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    return true;
}

bool catalog(const char *dirPath, void *param, void (*callback)(void *param, const char *name, const char *type, size_t size), int *err) {
    if (sd_card::g_testResult != TEST_OK) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    File dir = SD.open(dirPath);
    if (!dir) {
        if (err) *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    dir.rewindDirectory();

    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            return true;
        }

        char name[MAX_PATH_LENGTH + 1] = {0};
        entry.getName(name, MAX_PATH_LENGTH);
        if (entry.isDirectory()) {
            callback(param, name, "FOLD", entry.size());
        } else if (util::endsWith(name, list::LIST_EXT)) {
            callback(param, name, "LIST", entry.size());
        } else if (util::endsWith(name, profile::PROFILE_EXT)) {
            callback(param, name, "PROF", entry.size());
        } else {
            callback(param, name, "BIN", entry.size());
        }

        entry.close();
    }
}

bool catalogLength(const char *dirPath, size_t *length, int *err) {
    if (sd_card::g_testResult != TEST_OK) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    File dir = SD.open(dirPath);
    if (!dir) {
        if (err) *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    *length = 0;

    dir.rewindDirectory();

    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            break;
        }
        ++(*length);
        entry.close();
    }

    return true;
}

bool upload(const char *filePath, void *param, void (*callback)(void *param, const void *buffer, size_t size), int *err) {
    if (sd_card::g_testResult != TEST_OK) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    File file = SD.open(filePath, FILE_READ);

    if (!file) {
        if (err) *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    callback(param, NULL, file.size());

    const int CHUNK_SIZE = 64;
    uint8_t buffer[CHUNK_SIZE];

    while (true) {
        int size = file.read(buffer, CHUNK_SIZE);
        callback(param, buffer, size);
        if (size < CHUNK_SIZE) {
            break;
        }
    }

    file.close();

    callback(param, NULL, -1);

    return true;
}

bool download(const char *filePath, bool truncate, const void *buffer, size_t size, int *err) {
    if (sd_card::g_testResult != TEST_OK) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    File file = SD.open(filePath, FILE_WRITE);

    if (truncate && !file.truncate(0)) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    if (!file) {
        if (err) *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    size_t written = file.write((const uint8_t *)buffer, size);
    file.close();

    if (written != size) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    return true;
}

bool moveFile(const char *sourcePath, const char *destinationPath, int *err) {
    if (sd_card::g_testResult != TEST_OK) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    if (!SD.exists(sourcePath)) {
        if (err) *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    if (!SD.rename(sourcePath, destinationPath)) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    return true;
}

bool copyFile(const char *sourcePath, const char *destinationPath, int *err) {
    if (sd_card::g_testResult != TEST_OK) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    File sourceFile = SD.open(sourcePath, FILE_READ);

    if (!sourceFile) {
        if (err) *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    File destinationFile = SD.open(destinationPath, FILE_WRITE);

    if (!destinationFile) {
        sourceFile.close();
        if (err) *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

#if OPTION_DISPLAY
    gui::showProgressPage();
#endif

    const int CHUNK_SIZE = 512;
    uint8_t buffer[CHUNK_SIZE];
    size_t totalSize = sourceFile.size();
    size_t totalWritten = 0;

    while (true) {
        int size = sourceFile.read(buffer, CHUNK_SIZE);

        size_t written = destinationFile.write((const uint8_t *)buffer, size);
        if (size < 0 || written != (size_t)size) {
#if OPTION_DISPLAY
            gui::hideProgressPage();
#endif
            sourceFile.close();
            destinationFile.close();
            deleteFile(destinationPath, NULL);
            if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
            return false;
        }

        totalWritten += written;

#if OPTION_DISPLAY
        if (!gui::updateProgressPage(totalWritten, totalSize)) {
            sourceFile.close();
            destinationFile.close();

            deleteFile(destinationPath, NULL);
            if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;

            gui::hideProgressPage();
            return false;
        }
#endif

        if (size < CHUNK_SIZE) {
            break;
        }

        psu::tick();
    }

    sourceFile.close();
    destinationFile.close();

#if OPTION_DISPLAY
    gui::hideProgressPage();
#endif

    if (totalWritten != totalSize) {
        deleteFile(destinationPath, NULL);
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    return true;
}

bool deleteFile(const char *filePath, int *err) {
    if (sd_card::g_testResult != TEST_OK) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    if (!SD.exists(filePath)) {
        if (err) *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    if (!SD.remove(filePath)) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    return true;
}

bool makeDir(const char *dirPath, int *err) {
    if (sd_card::g_testResult != TEST_OK) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    if (!SD.mkdir(dirPath)) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    return true;
}

bool removeDir(const char *dirPath, int *err) {
    if (sd_card::g_testResult != TEST_OK) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    if (!SD.rmdir(dirPath)) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    return true;
}

void getDateTime(
    dir_t *d,
    uint8_t *resultYear, uint8_t *resultMonth, uint8_t *resultDay,
    uint8_t *resultHour, uint8_t *resultMinute, uint8_t *resultSecond
) {
    int year = FAT_YEAR(d->lastWriteDate);
    int month = FAT_MONTH(d->lastWriteDate);
    int day = FAT_DAY(d->lastWriteDate);

    int hour = FAT_HOUR(d->lastWriteTime);
    int minute = FAT_MINUTE(d->lastWriteTime);
    int second = FAT_SECOND(d->lastWriteTime);

    uint32_t utc = datetime::makeTime(year, month, day, hour, minute, second);
    uint32_t local = datetime::utcToLocal(utc, persist_conf::devConf.time_zone, (datetime::DstRule)persist_conf::devConf2.dstRule);
    datetime::breakTime(local, year, month, day, hour, minute, second);

    if (resultYear) {
        *resultYear = (uint8_t)(year - 2000);
        *resultMonth = (uint8_t)month;
        *resultDay = (uint8_t)day;
    }

    if (resultHour) {
        *resultHour = (uint8_t)hour;
        *resultMinute = (uint8_t)minute;
        *resultSecond = (uint8_t)second;
    }
}

bool getDate(const char *filePath, uint8_t &year, uint8_t &month, uint8_t &day, int *err) {
    if (sd_card::g_testResult != TEST_OK) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    File file = SD.open(filePath, FILE_READ);

    if (!file) {
        if (err) *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    dir_t d;
    bool result = file.dirEntry(&d);
    file.close();

    if (!result) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    getDateTime(&d, &year, &month, &day, NULL, NULL, NULL);

    return true;
}

bool getTime(const char *filePath, uint8_t &hour, uint8_t &minute, uint8_t &second, int *err) {
    if (sd_card::g_testResult != TEST_OK) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    if (!SD.exists(filePath)) {
        if (err) *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    File file = SD.open(filePath, FILE_READ);

    if (!file) {
        if (err) *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    dir_t d;
    bool result = file.dirEntry(&d);
    file.close();

    if (!result) {
        if (err) *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    getDateTime(&d, NULL, NULL, NULL, &hour, &minute, &second);

    return true;
}

bool getInfo(uint64_t &usedSpace, uint64_t &freeSpace) {
#ifdef EEZ_PSU_SIMULATOR
    return SD.getInfo(usedSpace, freeSpace);
#else
    uint64_t clusterCount = SD.vol()->clusterCount();
    uint64_t freeClusterCount = SD.vol()->freeClusterCount(psu::tick);
    usedSpace = 512 * (clusterCount - freeClusterCount) * SD.vol()->blocksPerCluster();
    freeSpace = 512 * freeClusterCount * SD.vol()->blocksPerCluster();
    return true;
#endif
}

}
}
} // namespace eez::psu::sd_card

#endif
