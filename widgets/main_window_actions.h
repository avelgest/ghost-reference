#pragma once

#include <QtCore/QObject>
#include <QtGui/QAction>

class MainWindow;

#define ACTION_DECL(name) \
private:                  \
    QAction m_##name;     \
                          \
public:                   \
    inline QAction &name() { return m_##name; }

class Actions : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Actions)

    ACTION_DECL(closeApplication);
    ACTION_DECL(minimizeApplication);
    ACTION_DECL(open);
    ACTION_DECL(toggleGhostMode);
    ACTION_DECL(save);

public:
    explicit Actions(MainWindow *mainWindow);
    ~Actions() override = default;

    MainWindow *mainWindow() const;
};

#undef ACTION_DECL
