/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2016-2021 John Baier <ebusd@ebusd.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIB_UTILS_ROTATEFILE_H_
#define LIB_UTILS_ROTATEFILE_H_

#include <unistd.h>
#include <stdint.h>
#include <iostream>
#include <fstream>
#include <string>

namespace ebusd {

/** \file lib/utils/rotatefile.h
 * Helpers for writing to rotating files.
 */

using std::string;

/**
 * Helper class for writing to a rotating file with maximum size.
 */
class RotateFile {
 public:
  /**
   * Construct a new instance.
   * @param fileName the name of the file write to.
   * @param maxSize the maximum size of the file to write to.
   * @param textMode whether to write each byte with prefixed timestamp and direction as text.
   * @param flushBuffer the size of the flush buffer.
   */
  RotateFile(const string fileName, const unsigned int maxSize, const bool textMode = false,
             const unsigned int flushBuffer = 16)
    : m_enabled(false), m_fileName(fileName), m_maxSize(maxSize), m_textMode(textMode), m_stream(), m_fileSize(0),
      m_flushSize(0), m_flushBuffer(flushBuffer) {}

  /**
   * Destructor.
   */
  virtual ~RotateFile();

  /**
   * Enable or disable writing to the file.
   * @param enabled @p true to enable writing to the file, @p false to disable it.
   * @return @p true when the state was changed, @p false otherwise.
   */
  bool setEnabled(bool enabled);

  /**
   * Return whether writing to the file is enabled.
   * @return whether writing to the file is enabled.
   */
  bool isEnabled() { return m_enabled; }

  /**
   * Write a number of bytes to the stream.
   * @param value the pointer to the bytes to write.
   * @param size the number of bytes to write.
   * @param received @a true on reception, @a false on sending (only relevant in text mode).
   * @param bytes whether to log single bytes (only relevant in text mode).
   */
  void write(const unsigned char* value, const size_t size, const bool received = true,
      const bool bytes = true);


 private:
  /** whether writing to the file is enabled. */
  bool m_enabled;

  /** the name of the file write to. */
  const string m_fileName;

  /** the maximum size of @a m_file, or 0 for infinite. */
  const unsigned int m_maxSize;

  /** whether to write each byte with prefixed timestamp and direction as text. */
  const bool m_textMode;

  /** the @a FILE to writing to. */
  FILE* m_stream;

  /** the number of bytes written to @a m_file. */
  uint64_t m_fileSize;

  /** the number of bytes written to @a m_file since the last flush. */
  uint64_t m_flushSize;

  /** the size of the flush buffer. */
  const unsigned int m_flushBuffer;
};

}  // namespace ebusd

#endif  // LIB_UTILS_ROTATEFILE_H_
