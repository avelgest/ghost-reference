#include "settings_panel.h"

#include <QtCore/Qt>

#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QSlider>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>

#include "../reference_image.h"

namespace
{
    const Qt::WindowFlags defaultWindowFlags = Qt::Tool;
    const QSize defaultSizeHint(256, 256);

    const int sliderScale = 100;
    const qreal sliderScaleF = static_cast<qreal>(sliderScale);

    struct minMax
    {
        qreal min = 0.;
        qreal max = 1.;
    };

    QSlider *createSlider(QWidget &parent,
                          const QString &label,
                          qreal value,
                          minMax range = {})
    {
        auto *layout = qobject_cast<QFormLayout *>(parent.layout());
        if (!layout)
        {
            qWarning() << "Expected parent to be a QFormLayout";
            return nullptr;
        }

        auto *slider = new QSlider(Qt::Horizontal, &parent);
        slider->setRange(static_cast<int>(range.min * sliderScale),
                         static_cast<int>(range.max * sliderScale));
        slider->setTracking(true);
        slider->setValue(static_cast<int>(value * sliderScale));

        layout->addRow(label, slider);
        return slider;
    }

    QToolButton *createFlipButton(SettingsPanel &parent, Qt::Orientation orientation)
    {
        const int btnIconSize = 40;
        const char *iconPath = (orientation == Qt::Vertical) ? "resources/flip_btn_v.png"
                                                             : "resources/flip_btn_h.png";
        auto *flipButton = new QToolButton(&parent);
        flipButton->setAutoRaise(true);
        flipButton->setIcon(QIcon(iconPath));
        flipButton->setIconSize({btnIconSize, btnIconSize});

        QObject::connect(flipButton, &QToolButton::clicked, &parent,
                         (orientation == Qt::Vertical) ? &SettingsPanel::flipImageVertically
                                                       : &SettingsPanel::flipImageHorizontally);
        return flipButton;
    }

    QToolBar *createToolBar(SettingsPanel &parent)
    {
        auto *toolBar = new QToolBar(&parent);
        QAction *action = nullptr;

        action = toolBar->addAction(QIcon("resources/flip_btn_h.png"), "Flip Horizontally");
        QObject::connect(action, &QAction::triggered, &parent, &SettingsPanel::flipImageHorizontally);

        action = toolBar->addAction(QIcon("resources/flip_btn_v.png"), "Flip Vertically");
        QObject::connect(action, &QAction::triggered, &parent, &SettingsPanel::flipImageVertically);

        return toolBar;
    }

} // namespace

SettingsPanel::SettingsPanel(QWidget *parent, const ReferenceImageSP &refImage)
    : QWidget(parent, {})
{
    setAutoFillBackground(true);
    setReferenceImage(refImage);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    if (!m_refImage)
    {
        setWindowTitle("Settings");
        auto *layout = new QGridLayout(this);

        auto *label = new QLabel("No Image", this);
        const int fontSize = 18;
        label->setAlignment(Qt::AlignCenter);
        label->setFont(QFont(label->font().families(), fontSize));
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        layout->addWidget(label);
        return;
    }

    setWindowTitle(refImage->name().isEmpty() ? "Settings" : refImage->name() + " Settings");

    auto *layout = new QGridLayout(this);
    QToolBar *toolBar = createToolBar(*this);
    layout->addWidget(toolBar, 0, 0, Qt::AlignTop);

    QWidget *settingsArea = initSettingsArea();
    layout->addWidget(settingsArea, 1, 0);

#if 0

    auto *layout = new QFormLayout(this);

    layout->addRow(createFlipButton(*this, Qt::Horizontal),
                   createFlipButton(*this, Qt::Vertical));

    QSlider *slider = nullptr;

    slider = createSlider(*this, "Opacity:", m_refImage->opacity());
    QObject::connect(slider, &QSlider::valueChanged, [this](int value)
                     { m_refImage->setOpacity(value / sliderScaleF); });

    slider = createSlider(*this, "Saturation:", m_refImage->saturation());
    QObject::connect(slider, &QSlider::valueChanged, [this](int value)
                     { m_refImage->setSaturation(value / sliderScaleF); });
#endif // 0
}

QSize SettingsPanel::sizeHint() const
{
    return defaultSizeHint;
}

void SettingsPanel::flipImageHorizontally() const
{
    m_refImage->setFlipHorizontal(!m_refImage->flipHorizontal());
}

void SettingsPanel::flipImageVertically() const
{
    m_refImage->setFlipVertical(!m_refImage->flipVertical());
}

QWidget *SettingsPanel::initSettingsArea()
{
    auto *scrollArea = new QScrollArea(this);
    auto *layout = new QFormLayout(scrollArea);

    QSlider *slider = nullptr;

    slider = createSlider(*scrollArea, "Opacity:", m_refImage->opacity());
    QObject::connect(slider, &QSlider::valueChanged, [this](int value)
                     { m_refImage->setOpacity(value / sliderScaleF); });

    slider = createSlider(*scrollArea, "Saturation:", m_refImage->saturation());
    QObject::connect(slider, &QSlider::valueChanged, [this](int value)
                     { m_refImage->setSaturation(value / sliderScaleF); });

    return scrollArea;
}
