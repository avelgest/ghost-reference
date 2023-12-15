#pragma once

#include <QtWidgets/QWidget>

#include "../types.h"

class SettingsPanel : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SettingsPanel)

    ReferenceImageSP m_refImage = nullptr;

public:
    explicit SettingsPanel(QWidget *parent = nullptr, const ReferenceImageSP &refImage = nullptr);
    ~SettingsPanel() override = default;

    const ReferenceImageSP &referenceImage() const;
    void setReferenceImage(const ReferenceImageSP &image);

    QSize sizeHint() const override;

public slots:
    void flipImageHorizontally() const;
    void flipImageVertically() const;

private:
    QWidget *initSettingsArea();
};

inline const ReferenceImageSP &SettingsPanel::referenceImage() const { return m_refImage; }

inline void SettingsPanel::setReferenceImage(const ReferenceImageSP &image) { m_refImage = image; }
