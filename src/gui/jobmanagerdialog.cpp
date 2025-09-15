/**
 * @file   jobmanagerdialog.cpp
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

#include "jobmanagerdialog.h"
#include "ui_jobmanagerdialog.h"

#include <QtWidgets>

#include "context.h"
#include "jobmatrix.h"
#include "logger.h"
#include "settings.h"
#include "structuredtextparser.h"

JobManagerDialog::JobManagerDialog(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::JobManagerDialog)
{
  LOG_STEP("JobManagerDialog constructor");
  ui->setupUi(this);

  LOG_STEP("Getting root directory from settings");
  m_rootDirName = SETTINGS->get("System", "RootDir").toString();

  if (m_rootDirName.isEmpty()) {
    m_rootDirName = QCoreApplication::applicationDirPath() + "/Jobs";
    SETTINGS->set("System", "RootDir", m_rootDirName);
    LOG_INFO(QString("Set default root directory: %1").arg(m_rootDirName));
  } else {
    LOG_INFO(QString("Using root directory: %1").arg(m_rootDirName));
  }

  QDir jobsDir(m_rootDirName);
  if (!jobsDir.exists()) {
    LOG_STEP("Creating jobs directory");
    if (QDir::root().mkdir(m_rootDirName)) {
      LOG_INFO("Jobs directory created successfully");
    } else {
      LOG_ERROR("Failed to create jobs directory");
    }
  }

  LOG_STEP("Setting up file system model");
  m_model = new QFileSystemModel;
  m_model->setRootPath(m_rootDirName);

  ui->listView->setModel(m_model);
  ui->listView->setRootIndex(m_model->index(m_rootDirName));
  LOG_INFO("JobManagerDialog initialization complete");
}

JobManagerDialog::~JobManagerDialog()
{
  LOG_STEP("JobManagerDialog destructor");
  delete ui;
  delete m_model;
}

void JobManagerDialog::on_browseButton_clicked(void)
{
  LOG_STEP("Browse button clicked");
  QFileDialog diag(NULL, "Choose a tarball", "",
      "ODB++ database (*.tgz *.tar.gz)");
  diag.exec();

  QStringList files = diag.selectedFiles();
  if (files.isEmpty()) {
    LOG_INFO("No file selected");
    return;
  }

  QString sel = files[0];
  LOG_INFO(QString("Selected file: %1").arg(sel));
  
  if (sel.endsWith(".tgz") || sel.endsWith(".tar.gz")) {
    ui->filenameLineEdit->setText(sel);
    LOG_INFO("Valid ODB++ file selected");
  } else {
    LOG_WARNING("Invalid file type selected");
  }
}

void JobManagerDialog::on_importButton_clicked(void)
{
  LOG_STEP("Import button clicked");
  QString filename = ui->filenameLineEdit->text();

  if (filename.isEmpty()) {
    LOG_ERROR("No filename specified");
    QMessageBox::critical(this, "Error", "No filename specified!");
    return;
  }

  LOG_INFO(QString("Importing file: %1").arg(filename));
  QString jobName = QFileInfo(filename).baseName();
  LOG_INFO(QString("Job name: %1").arg(jobName));

  QDir jobsDir(m_rootDirName);
  if (!jobsDir.mkdir(jobName)) {
    LOG_ERROR(QString("Job directory already exists: %1").arg(jobName));
    QMessageBox::critical(this, "Error", "Job with same name exists!");
    return;
  }

  QString extractDir = jobsDir.absoluteFilePath(jobName);
  LOG_INFO(QString("Extract directory: %1").arg(extractDir));

  // Decompress tarball
  LOG_STEP("Preparing to decompress main tarball");
  QStringList args;

#ifdef Q_WS_WIN
  filename.replace(":", "");
  filename.prepend('/');
  LOG_INFO(QString("Windows path adjusted: %1").arg(filename));
#endif

  args << "xf" << filename << "--strip-components=1" << "-C" << extractDir;
  LOG_INFO(QString("TAR command: %1 %2").arg(TAR_CMD, args.join(" ")));
  LOG_DEBUG(QString("TAR command (raw): %1 xf %2 --strip-components=1 -C %3")
      .arg(TAR_CMD).arg(filename).arg(extractDir));

  QMessageBox msg(QMessageBox::Information, "Progress",
      "Decompressing archive...");
  msg.setStandardButtons(QMessageBox::NoButton);
  msg.show();

  LOG_STEP("Executing TAR extraction");
  int ret = execute(TAR_CMD, args);

  msg.hide();

  if (ret != 0) {
    LOG_ERROR(QString("TAR extraction failed with code: %1").arg(ret));
    QMessageBox::critical(this, "Error",
        QString("Error when decompressing `%1'").arg(filename));
    recurRemove(extractDir);
    return;
  }
  LOG_INFO("TAR extraction completed successfully");

  // Decompress all layers
  LOG_STEP("Starting layer decompression");
  msg.setText("Decompressing all layers...");
  msg.show();

  QString matrix = extractDir + "/matrix/matrix";
  LOG_INFO(QString("Parsing matrix file: %1").arg(matrix));

  StructuredTextParser parser(matrix);
  StructuredTextDataStore* ds = parser.parse();

  if (ds == NULL) {
    LOG_ERROR("Failed to parse matrix file - invalid ODB++ database");
    QMessageBox::critical(this, "Error",
        QString("`%1' is not a valid ODB++ database.").arg(filename));
    recurRemove(extractDir);
    return;
  }
  LOG_INFO("Matrix file parsed successfully");

  StructuredTextDataStore::BlockIterPair ip;
  QStringList steps, layers;

  LOG_STEP("Extracting step information");
  ip = ds->getBlocksByKey("STEP");
  for (StructuredTextDataStore::BlockIter it = ip.first; it != ip.second; ++it)
  {
    QString stepName = QString::fromStdString(it->second->get("NAME")).toLower();
    steps.append(stepName);
    LOG_INFO(QString("Found step: %1").arg(stepName));
  }

  LOG_STEP("Extracting layer information");
  ip = ds->getBlocksByKey("LAYER");
  for (StructuredTextDataStore::BlockIter it = ip.first; it != ip.second; ++it)
  {
    QString layerName = QString::fromStdString(it->second->get("NAME")).toLower();
    layers.append(layerName);
    LOG_INFO(QString("Found layer: %1").arg(layerName));
  }

  LOG_INFO(QString("Total steps: %1, Total layers: %2").arg(steps.size()).arg(layers.size()));

  QString layerPathTmpl = extractDir + "/steps/%1/layers/%2/features";
  int totalFiles = 0;
  int processedFiles = 0;
  
  // Count total compressed files
  for (int i = 0; i < steps.size(); ++i) {
    for (int j = 0; j < layers.size(); ++j) {
      QString path = layerPathTmpl.arg(steps[i]).arg(layers[j]);
      if (QFile(path + ".Z").exists() || QFile(path + ".z").exists()) {
        totalFiles++;
      }
    }
  }
  
  LOG_INFO(QString("Found %1 compressed layer files to decompress").arg(totalFiles));

  for (int i = 0; i < steps.size(); ++i) {
    for (int j = 0; j < layers.size(); ++j) {
      QString path = layerPathTmpl.arg(steps[i]).arg(layers[j]);
      QString gzFilename;
      if (QFile(path + ".Z").exists()) {
        gzFilename = path + ".Z";
      } else if (QFile(path + ".z").exists()) {
        gzFilename = path + ".z";
      } else {
        continue;
      }

      processedFiles++;
      LOG_PROGRESS("Decompressing layers", processedFiles, totalFiles);
      
      msg.setText(QString("Decompressing %1/%2 ...")
          .arg(steps[i]).arg(layers[j]));

      LOG_INFO(QString("Decompressing: %1").arg(gzFilename));
      int ret = executeGzipDecompression(gzFilename);

      if (ret != 0) {
        LOG_ERROR(QString("Decompression failed: %1 (code: %2)").arg(gzFilename).arg(ret));
        QMessageBox::critical(this, "Error",
            QString("Error when decompressing `%1'").arg(gzFilename));
        recurRemove(extractDir);
        msg.hide();
        return;
      }
    }
  }

  msg.hide();
  LOG_STEP("Import completed successfully");
  LOG_INFO(QString("Job '%1' imported with %2 steps and %3 layers").arg(jobName).arg(steps.size()).arg(layers.size()));
}

void JobManagerDialog::on_removeButton_clicked(void)
{
  LOG_STEP("Remove button clicked");
  QString name = m_model->data(ui->listView->currentIndex()).toString();
  LOG_INFO(QString("Attempting to remove job: %1").arg(name));

  int ret = QMessageBox::question(this,"Confirm",
      QString("Are you sure you want to remove `%1'").arg(name),
        QMessageBox::Yes, QMessageBox::No);

  if (ret != QMessageBox::Yes) {
    LOG_INFO("Job removal cancelled by user");
    return;
  }

  QString jobPath = m_rootDirName + "/" + name;
  LOG_STEP(QString("Removing job directory: %1").arg(jobPath));
  
  if (recurRemove(jobPath)) {
    LOG_INFO("Job removed successfully");
  } else {
    LOG_ERROR("Failed to remove job directory");
  }
}

void JobManagerDialog::on_setRootButton_clicked(void)
{
  LOG_STEP("Set root button clicked");
  QFileDialog diag(NULL, "Choose a directory", m_rootDirName);
  diag.setFileMode(QFileDialog::Directory);
  diag.setOption(QFileDialog::ShowDirsOnly);

  if (diag.exec()) {
    m_rootDirName = diag.selectedFiles()[0];
    SETTINGS->set("System", "RootDir", m_rootDirName);
    LOG_INFO(QString("Root directory changed to: %1").arg(m_rootDirName));

    m_model->setRootPath(m_rootDirName);
    ui->listView->setModel(m_model);
    ui->listView->setRootIndex(m_model->index(m_rootDirName));
    LOG_INFO("File system model updated");
  } else {
    LOG_INFO("Root directory change cancelled");
  }
}

void JobManagerDialog::on_listView_doubleClicked(const QModelIndex& index)
{
  QString name = m_model->data(index).toString();
  LOG_STEP(QString("Opening job: %1").arg(name));
  
  QString jobPath = m_rootDirName + "/" + name;
  LOG_INFO(QString("Job path: %1").arg(jobPath));
  
  ctx.loader = new ArchiveLoader(jobPath);
  LOG_INFO("ArchiveLoader created");

  try {
    JobMatrix* job = new JobMatrix(name);
    LOG_INFO("JobMatrix created successfully");
    job->show();
    hide();
    LOG_STEP("JobMatrix displayed, JobManagerDialog hidden");
  } catch (const std::exception& e) {
    LOG_ERROR(QString("Exception creating JobMatrix: %1").arg(e.what()));
  } catch (...) {
    LOG_ERROR("Unknown exception creating JobMatrix");
  }
}

int JobManagerDialog::execute(QString cmd, QStringList args)
{
  LOG_INFO(QString("Executing command: %1 %2").arg(cmd, args.join(" ")));
  
  QEventLoop loop;

  QProcess process;
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("PATH", env.value("PATH") +
      PATH_SEP + QCoreApplication::applicationDirPath());
  process.setProcessEnvironment(env);
  
  // Connect process output for debugging
  connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          [&](int exitCode, QProcess::ExitStatus exitStatus) {
              LOG_INFO(QString("Process finished - Exit code: %1, Status: %2")
                      .arg(exitCode)
                      .arg(exitStatus == QProcess::NormalExit ? "Normal" : "Crashed"));
              loop.quit();
          });

  connect(&process, &QProcess::errorOccurred,
          [&](QProcess::ProcessError error) {
              LOG_ERROR(QString("Process error occurred: %1").arg(error));
          });

  process.start(cmd, args);
  
  if (!process.waitForStarted()) {
    LOG_ERROR(QString("Failed to start process: %1").arg(process.errorString()));
    return -1;
  }
  
  LOG_INFO("Process started successfully");
  loop.exec();

  return process.exitCode();
}

bool JobManagerDialog::recurRemove(const QString& dirname)
{
  LOG_INFO(QString("Recursively removing directory: %1").arg(dirname));
  bool result = true;
  QDir dir(dirname);

  if (dir.exists(dirname)) {
    Q_FOREACH(QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot |
          QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files,
          QDir::DirsFirst)) {
      if (info.isDir()) {
        result = recurRemove(info.absoluteFilePath());
      }
      else {
        result = QFile::remove(info.absoluteFilePath());
        if (result) {
          LOG_INFO(QString("Removed file: %1").arg(info.absoluteFilePath()));
        } else {
          LOG_ERROR(QString("Failed to remove file: %1").arg(info.absoluteFilePath()));
        }
      }

      if (!result) {
        return result;
      }
    }
    result = dir.rmdir(dirname);
    if (result) {
      LOG_INFO(QString("Removed directory: %1").arg(dirname));
    } else {
      LOG_ERROR(QString("Failed to remove directory: %1").arg(dirname));
    }
  }
  return result;
}

int JobManagerDialog::executeGzipDecompression(QString filePath)
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
  
  int ret = execute("7z", sevenZipArgs);
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
    
    ret = execute(gzipPath, gzipArgs);
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
  
  ret = execute("gzip", gzipArgs);
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
  
  int ret = execute("gzip", gzipArgs);
  if (ret == 0) {
    LOG_INFO("Gzip decompression successful");
    return 0;
  }
  LOG_WARNING(QString("Gzip failed with code: %1").arg(ret));
#endif

  LOG_ERROR("All decompression methods failed");
  return -1;
}
