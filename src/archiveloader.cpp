/**
 * @file   archiveloader.cpp
 * @author Wei-Ning Huang (AZ) <aitjcize@gmail.com>
 *
 * Copyright (C) 2012 - 2014 Wei-Ning Huang (AZ) <aitjcize@gmail.com>
 * All Rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "archiveloader.h"

#include <QtCore>
#include <QFileInfo>
#include "logger.h"
#include "parser.h"

ArchiveLoader::ArchiveLoader(QString filename): m_fileName(filename)
{
  m_dir = QDir(filename);
}

ArchiveLoader::~ArchiveLoader()
{
}

QString ArchiveLoader::absPath(QString path)
{
  return m_dir.absoluteFilePath(path);
}

QStringList ArchiveLoader::listDir(QString filename)
{
  QDir dir(m_dir.absoluteFilePath(filename));
  return dir.entryList(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files);
}

QString ArchiveLoader::featuresPath(QString base)
{
  QString plain_path = absPath(base.toLower() + "/features");
  LOG_INFO(QString("ArchiveLoader::featuresPath() - Base: %1, Plain path: %2").arg(base, plain_path));

  // Ensure feature is not a gzipped file. If so, unzip it.
  QString path = plain_path;
  QFile file(path);

  if (!file.exists()) { // try .[zZ]
    LOG_INFO(QString("Features file not found at: %1, trying compressed versions").arg(path));
    
    file.setFileName(path + ".Z");
    if (file.exists()) {
      path += ".Z";
      LOG_INFO(QString("Found compressed file: %1").arg(path));
    } else {
      file.setFileName(path + ".z");
      if (!file.exists()) {
        LOG_ERROR(QString("No features file found: %1, %2.Z, %2.z").arg(plain_path, plain_path));
        return QString();
      }
      path += ".z";
      LOG_INFO(QString("Found compressed file: %1").arg(path));
    }

    LOG_STEP(QString("Decompressing features file: %1").arg(path));
    int ret = executeGzipDecompression(path);
    LOG_INFO(QString("Decompression result: %1").arg(ret));

    if (ret != 0) {
      LOG_ERROR(QString("Decompression failed with code: %1").arg(ret));
      return QString();
    }
    LOG_INFO("Decompression completed successfully");
  } else {
    LOG_INFO(QString("Features file already exists: %1").arg(path));
  }

  // Check final file
  QFileInfo finalFileInfo(plain_path);
  if (finalFileInfo.exists()) {
    LOG_INFO(QString("Final features file - Size: %1 bytes, Readable: %2")
            .arg(finalFileInfo.size())
            .arg(finalFileInfo.isReadable() ? "Yes" : "No"));
  } else {
    LOG_ERROR(QString("Final features file does not exist: %1").arg(plain_path));
  }

  return plain_path;
}

int ArchiveLoader::executeGzipDecompression(QString filePath)
{
  LOG_INFO(QString("Attempting decompression of: %1").arg(filePath));
  
  // Debug: Check which platform we're on
#ifdef Q_WS_WIN
  LOG_INFO("Platform detected: Windows (Q_WS_WIN defined)");
#elif defined(Q_OS_WIN)
  LOG_INFO("Platform detected: Windows (Q_OS_WIN defined)");
#else
  LOG_INFO("Platform detected: Unix/Linux");
#endif
  
#if defined(Q_WS_WIN) || defined(Q_OS_WIN)
  // Primary method: Use 7-zip on Windows
  LOG_INFO("Using 7-zip for decompression (Windows)");
  QStringList sevenZipArgs;
  sevenZipArgs << "x" << filePath << "-y" << QString("-o%1").arg(QFileInfo(filePath).absolutePath());
  
  int ret = QProcess::execute("7z", sevenZipArgs);
  if (ret == 0) {
    LOG_INFO("7-zip decompression successful");
    // Remove the compressed file after successful extraction
    if (QFile::remove(filePath)) {
      LOG_INFO("Compressed file removed successfully");
    } else {
      LOG_WARNING("Failed to remove compressed file");
    }
    return 0;
  }
  LOG_WARNING(QString("7-zip failed with code: %1").arg(ret));
  
  // Fallback 1: Try native gzip if available
  QString gzipPath = QCoreApplication::applicationDirPath() + "/gzip.exe";
  if (QFile::exists(gzipPath)) {
    LOG_INFO("Trying local gzip as fallback");
    QStringList gzipArgs;
    gzipArgs << "-d" << filePath;
    
    ret = QProcess::execute(gzipPath, gzipArgs);
    if (ret == 0) {
      LOG_INFO("Local gzip decompression successful");
      return 0;
    }
    LOG_WARNING(QString("Local gzip failed with code: %1").arg(ret));
  }
  
  // Fallback 2: Try system gzip
  LOG_INFO("Trying system gzip as fallback");
  QStringList gzipArgs;
  gzipArgs << "-d" << filePath;
  
  ret = QProcess::execute("gzip", gzipArgs);
  if (ret == 0) {
    LOG_INFO("System gzip decompression successful");
    return 0;
  }
  LOG_WARNING(QString("System gzip failed with code: %1").arg(ret));
  
#else
  // On Linux/Unix, use standard gzip
  LOG_INFO("Using system gzip (Unix/Linux)");
  QStringList gzipArgs;
  gzipArgs << "-d" << filePath;
  
  int ret = QProcess::execute("gzip", gzipArgs);
  if (ret == 0) {
    LOG_INFO("Gzip decompression successful");
    return 0;
  }
  LOG_WARNING(QString("Gzip failed with code: %1").arg(ret));
#endif

  LOG_ERROR("All decompression methods failed");
  return -1;
}
