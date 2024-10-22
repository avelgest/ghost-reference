#pragma once

#include <QtCore/QAbstractAnimation>
#include <QtCore/QScopedPointer>

#include <QtWidgets/QWidget>

#include "../types.h"

class QGraphicsOpacityEffect;

class BackWindow;
class BackWindowActions;

class MainToolbar : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MainToolbar)

    friend class BackWindowActions;

    class FadeStartTimer;

    using GraphicsEffect = QGraphicsOpacityEffect;

    QScopedPointer<QAbstractAnimation> m_anim;
    GraphicsEffect *m_graphicsEffect;
    QWidget *m_dragWidget = nullptr;
    bool m_expanded = true;
    FadeStartTimer *const m_fadeStartTimer;

public:
    explicit MainToolbar(QWidget *parent = nullptr);
    ~MainToolbar() override = default;

    static BackWindow *backWindow();

    BackWindowActions *backWindowActions();

    bool expanded() const;
    void setExpanded(bool value);

    qreal opacity() const;
    void setOpacity(qreal value);
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

    QSize sizeHint() const override;

protected:
    void actionEvent(QActionEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

    static ReferenceWindow *newReferenceWindow(const ReferenceImageSP &refItem);
    static ReferenceWindow *newReferenceWindow(const QList<ReferenceImageSP> &loadResults);

    QAction *addSeparator();
    QWidget *addWidgetFor(QAction *action);
    bool removeWidgetFor(const QAction *action);

    QAbstractAnimation *anim() const;
    void setAnim(QAbstractAnimation *anim);
};

inline bool MainToolbar::expanded() const { return m_expanded; }
inline QAbstractAnimation *MainToolbar::anim() const { return m_anim.get(); }
