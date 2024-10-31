#include "logger.h"

#include <iostream>

#include <QtCore/QDir>
#include <QtCore/QPointer>
#include <QtCore/QSaveFile>

#include "app.h"
#include "preferences.h"

namespace
{
    const char *logFileName = "ghost_reference_log.txt";

    void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
    {
        if (Logger *logger = Logger::activeLogger(); logger)
        {
            emit logger->messageSignal(qFormatLogMessage(type, ctx, msg) + "\n");
        }
    }

} // namespace

QPointer<Logger> Logger::s_activeLogger = nullptr;

Logger::Logger(QObject *parent)
    : QObject(parent),
      m_oldLogger(activeLogger())
{
    m_oldHandler = qInstallMessageHandler(messageHandler);

    qSetMessagePattern("[%{type}]\t%{message}");

    setActiveLogger(this);

    QObject::connect(this, &Logger::messageSignal, this, &Logger::handleMessage, Qt::QueuedConnection);
    QObject::connect(App::ghostRefInstance(), &App::preferencesReplaced, this,
                     [this](Preferences *prefs) { setUseLogFile(prefs->getBool(Preferences::LoggingEnabled)); });
}

Logger::~Logger()
{
    if (m_oldHandler)
    {
        qInstallMessageHandler(m_oldHandler);
    }
    if (m_oldLogger)
    {
        setActiveLogger(m_oldLogger);
    }
    if (usesLogFile())
    {
        m_file->commit();
    }
}

Logger *Logger::activeLogger()
{
    return s_activeLogger;
}

QString Logger::logFilePath()
{
    const QDir configDir(Preferences::configDir());
    return configDir.absoluteFilePath(logFileName);
}

void Logger::removeOldLogFiles()
{
    QDir configDir(Preferences::configDir());
    configDir.setFilter(QDir::Filter::Files);
    configDir.setNameFilters({QString(logFileName) + ".*"});

    for (const auto &filename : configDir.entryList())
    {
        if (filename.startsWith(logFileName))
        {
            configDir.remove(filename);
        }
    }
}

bool Logger::usesLogFile()
{
    return m_file != nullptr;
}

void Logger::setUseLogFile(bool value)
{
    if (usesLogFile() == value)
    {
        return;
    }

    if (value)
    {
        m_file = std::make_unique<QSaveFile>(logFilePath());
        m_file->open(QSaveFile::WriteOnly);
    }
    else
    {
        m_file.reset();
    }
    Q_ASSERT(usesLogFile() == value);
}

void Logger::setActiveLogger(Logger *logger)
{
    s_activeLogger = logger;
}

void Logger::handleMessage(QString msg)
{
    std::string stdString = msg.toStdString();
    std::cerr << stdString;
    if (usesLogFile())
    {
        m_file->write(stdString.data(), static_cast<qint64>(stdString.size()));
    }
}
