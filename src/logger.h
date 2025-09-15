/**
 * @file   logger.h
 * @author Modified for QCamber debugging
 * 
 * Simple logging system for QCamber application debugging
 */

#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <QString>
#include <QTextStream>
#include <QDebug>
#include <QDateTime>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>
// Undefine Windows ERROR macro to avoid conflicts
#ifdef ERROR
#undef ERROR
#endif
#endif

class Logger {
public:
    enum LogLevel {
        LOG_DEBUG = 0,
        LOG_INFO = 1,
        LOG_WARNING = 2,
        LOG_ERROR = 3
    };

    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void initConsole() {
#ifdef Q_OS_WIN
        // Allocate a console for this GUI application
        if (AllocConsole()) {
            // Redirect stdout, stdin, stderr to console
            freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
            freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
            freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
            
            // Make cout, wcout, cin, wcin, wcerr, cerr, wclog and clog
            // point to console as well
            std::ios::sync_with_stdio(true);
            
            // Set console title
            SetConsoleTitle(L"QCamber Debug Console");
            
            m_consoleActive = true;
            log(LOG_INFO, "Debug console initialized successfully");
        }
#else
        m_consoleActive = true;
        log(LOG_INFO, "Console logging enabled");
#endif
    }

    void log(LogLevel level, const QString& message) {
        if (!m_consoleActive) return;

        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
        QString levelStr = levelToString(level);
        QString fullMessage = QString("[%1] %2: %3").arg(timestamp, levelStr, message);
        
        // Output to console
        std::cout << fullMessage.toStdString() << std::endl;
        
        // Also use Qt's debug system
        qDebug() << fullMessage;
    }

    void logStep(const QString& stepName, const QString& details = QString()) {
        QString msg = QString("STEP: %1").arg(stepName);
        if (!details.isEmpty()) {
            msg += QString(" - %1").arg(details);
        }
        log(LOG_INFO, msg);
    }

    void logError(const QString& error, const QString& context = QString()) {
        QString msg = error;
        if (!context.isEmpty()) {
            msg = QString("%1 (Context: %2)").arg(error, context);
        }
        log(LOG_ERROR, msg);
    }

    void logProgress(const QString& operation, int current, int total) {
        QString msg = QString("%1: %2/%3 (%4%)")
            .arg(operation)
            .arg(current)
            .arg(total)
            .arg(total > 0 ? (current * 100 / total) : 0);
        log(LOG_INFO, msg);
    }

private:
    Logger() : m_consoleActive(false) {}
    
    QString levelToString(LogLevel level) {
        switch (level) {
            case LOG_DEBUG: return "DEBUG";
            case LOG_INFO: return "INFO ";
            case LOG_WARNING: return "WARN ";
            case LOG_ERROR: return "ERROR";
            default: return "UNKN ";
        }
    }

    bool m_consoleActive;
};

// Convenience macros for easy logging
#define LOG_DEBUG(msg) Logger::instance().log(Logger::LOG_DEBUG, msg)
#define LOG_INFO(msg) Logger::instance().log(Logger::LOG_INFO, msg)
#define LOG_WARNING(msg) Logger::instance().log(Logger::LOG_WARNING, msg)
#define LOG_ERROR(msg) Logger::instance().log(Logger::LOG_ERROR, msg)
#define LOG_STEP(step, ...) Logger::instance().logStep(step, ##__VA_ARGS__)
#define LOG_ERROR_CTX(error, context) Logger::instance().logError(error, context)
#define LOG_PROGRESS(op, cur, total) Logger::instance().logProgress(op, cur, total)

#endif /* __LOGGER_H__ */