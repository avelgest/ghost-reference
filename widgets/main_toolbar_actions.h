#pragma once

#include <QtCore/QObject>
#include <QtGui/QAction>
#include <QtGui/QActionGroup>

class MainToolbar;

#define ACTION_DECL(name) \
private:                  \
    QAction m_##name;     \
                          \
public:                   \
    inline QAction &name() { return m_##name; }

class MainToolbarActions : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MainToolbarActions)

    ACTION_DECL(closeApplication);
    ACTION_DECL(minimizeToolbar);
    ACTION_DECL(openSession);
    ACTION_DECL(paste);
    ACTION_DECL(toggleAllRefsHidden);
    ACTION_DECL(toggleGhostMode);
    ACTION_DECL(redo);
    ACTION_DECL(saveSession);
    ACTION_DECL(saveSessionAs);
    ACTION_DECL(showHelp);
    ACTION_DECL(showPreferences);
    ACTION_DECL(undo);

private:
    QActionGroup m_windowModeGroup;

public:
    explicit MainToolbarActions(MainToolbar *mainToolbar);
    ~MainToolbarActions() override = default;

    MainToolbar *mainToolbar() const;
};

#undef ACTION_DECL
