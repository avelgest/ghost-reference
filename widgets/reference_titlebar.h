#pragma once

#include <QtWidgets/QToolBar>

class ReferenceWindow;

class ReferenceTitlebar : public QToolBar
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ReferenceTitlebar)

public:
    explicit ReferenceTitlebar(ReferenceWindow *parent = nullptr);
    ~ReferenceTitlebar() override = default;

    void showSettingsPanel() const;

    ReferenceWindow *referenceWindow() const;

    QSize sizeHint() const override;

signals:
    void moved(QPoint diff);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void initActions();

    void closeWindow() const;
    void minimizeWindow() const;
};
