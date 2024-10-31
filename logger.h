#pragma once

#include <QtCore/QMessageLogger>
#include <QtCore/QObject>
#include <QtCore/QPointer>

class QSaveFile;

class Logger : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Logger)

    static QPointer<Logger> s_activeLogger;

    std::unique_ptr<QSaveFile> m_file;
    QtMessageHandler m_oldHandler = nullptr;
    QPointer<Logger> m_oldLogger = nullptr;

public:
    explicit Logger(QObject *parent = nullptr);
    ~Logger() override;

    static Logger *activeLogger();
    static QString logFilePath();

    bool usesLogFile();
    void setUseLogFile(bool value);

protected:
    static void setActiveLogger(Logger *logger);

    void handleMessage(QString msg);

signals:
    void messageSignal(QString msg);
};
