#pragma once

#include <optional>

#include <QtCore/QObject>
#include <QtGui/QCursor>

class App;
class QContextMenuEvent;
class QKeyEvent;
class QMouseEvent;

class Tool : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Tool)

private:
    static Tool *s_activeTool;

    std::optional<QCursor> m_cursor;

public:
    template <class T> static T *activateTool();
    static Tool *activeTool();

    Tool() = default;
    ~Tool() override = default;

    bool eventFilter(QObject *watched, QEvent *event) override;

    bool isActive() const;
    void deactivate();

    std::optional<QCursor> cursor();

protected:
    void setCursor(const std::optional<QCursor> &cursor);

    static QWidgetList findReferenceWidgets();

    virtual void onActivate();
    virtual void onDeactivate();

    virtual void mouseMoveEvent(QWidget *widget, QMouseEvent *event);
    virtual void mousePressEvent(QWidget *widget, QMouseEvent *event);
    virtual void mouseReleaseEvent(QWidget *widget, QMouseEvent *event);

    virtual void contextMenuEvent(QWidget *widget, QContextMenuEvent *event);

    virtual void keyPressEvent(QWidget *widget, QKeyEvent *event);
    virtual void keyReleaseEvent(QWidget *widget, QKeyEvent *event);

private:
    void onActivatePrivate();
    void onDeactivatePrivate();
};

inline Tool *Tool::s_activeTool = nullptr;

inline bool Tool::isActive() const
{
    return activeTool() == this;
}

inline std::optional<QCursor> Tool::cursor()
{
    return m_cursor;
}

template <class T> inline T *Tool::activateTool()
{
    if (s_activeTool)
    {
        s_activeTool->deactivate();
    }
    T *tool = new T();
    s_activeTool = tool;
    s_activeTool->onActivatePrivate();

    return tool;
}

inline Tool *Tool::activeTool()
{
    return s_activeTool;
}

inline void Tool::onActivate() {}

inline void Tool::onDeactivate() {}

inline void Tool::mouseMoveEvent(QWidget *widget, QMouseEvent *event) {}

inline void Tool::mousePressEvent(QWidget *widget, QMouseEvent *event) {}

inline void Tool::mouseReleaseEvent(QWidget *widget, QMouseEvent *event) {}

inline void Tool::keyPressEvent(QWidget *widget, QKeyEvent *event) {}

inline void Tool::keyReleaseEvent(QWidget *widget, QKeyEvent *event) {}
