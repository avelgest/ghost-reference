#pragma once

#include <QtWidgets/QSystemTrayIcon>

#include <QtWidgets/QMenu>

class SystemTrayIcon : public QSystemTrayIcon
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SystemTrayIcon)

private:
    QMenu m_contextMenu;

public:
    explicit SystemTrayIcon(QObject *parent = nullptr);
    ~SystemTrayIcon() override = default;

private:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
};