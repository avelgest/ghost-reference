#pragma once

#include <QtWidgets/QTabBar>

#include "../types.h"

class TabBar : public QTabBar
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TabBar)

    ReferenceWindow *m_parent;

public:
    explicit TabBar(ReferenceWindow *parent);
    ~TabBar() override = default;

    int indexOf(const ReferenceImageSP &refItem);
    ReferenceImageSP referenceAt(int index) const;

protected:
    void onActiveImageChanged(const ReferenceImageSP &refItem);
    void onReferenceAdded(const ReferenceImageSP &refItem);
    void onReferenceRemoved(const ReferenceImageSP &refItem);
    void onCurrentChanged(int index);
    void onTabCloseRequested(int index);

    QWidget *createButtonWidget(const ReferenceImageSP &refItem);
    void removeReference(int index);

    QSize tabSizeHint(int index) const override;
    void hideEvent(QHideEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void adjustParentSize();
    void onWindowModeChanged(WindowMode windowMode);
};
