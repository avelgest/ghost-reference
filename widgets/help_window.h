#pragma once

#include <QtWidgets/QWidget>

class HelpWindow : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(HelpWindow)

private:
    int m_currentTipIdx = 0;
    QList<QString> m_tips;

public:
    explicit HelpWindow(QWidget *parent = nullptr);
    ~HelpWindow() override = default;

protected:
    int currentTipIdx() const;
    void setCurrentTipIdx(int idx);

    const QList<QString> &tips();

signals:
    void currentTipIdxChanged(int idx);
};

inline int HelpWindow::currentTipIdx() const
{
    return m_currentTipIdx;
}