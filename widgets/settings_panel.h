#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include "../types.h"

class QToolBar;

class SettingsPanel : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SettingsPanel)

private:
    QPointer<ReferenceWindow> m_refWindow;
    ReferenceImageSP m_refImage = nullptr;

    QWidget *m_noRefWidget = nullptr;
    QWidget *m_settingsArea = nullptr;
    QToolBar *m_toolBar = nullptr;

public:
    explicit SettingsPanel(ReferenceWindow *refWindow, QWidget *parent = nullptr);
    ~SettingsPanel() override = default;

    ReferenceWindow *refWindow() const;
    void setRefWindow(ReferenceWindow *refWindow);

    const ReferenceImageSP &referenceImage() const;
    void setReferenceImage(const ReferenceImageSP &image);

    QWidget *settingsArea() const;
    QToolBar *toolBar() const;

    QSize sizeHint() const override;

signals:
    // Emitted whenever referenceImage is set (even if set to the same value).
    void refImageChanged(const ReferenceImageSP &image);

public slots:
    void flipImageHorizontally() const;
    void flipImageVertically() const;

private:
    void initNoRefWidget();
    void initSettingsArea();
    void buildInterface();
    void refreshInterface();
};

inline ReferenceWindow *SettingsPanel::refWindow() const { return m_refWindow; }

inline const ReferenceImageSP &SettingsPanel::referenceImage() const { return m_refImage; }

inline QWidget *SettingsPanel::settingsArea() const { return m_settingsArea; }

inline QToolBar *SettingsPanel::toolBar() const { return m_toolBar; }
