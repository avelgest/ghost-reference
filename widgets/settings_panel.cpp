#include "settings_panel.h"

#include <initializer_list>
#include <utility>

#include <QtCore/Qt>

#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QSlider>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>

#include "../reference_image.h"
#include "reference_window.h"

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

    template <typename Func1, typename Func2>
    QSlider *createSlider(SettingsPanel *settingsPanel,
                          const QString &label,
                          Func1 getter,
                          Func2 setter,
                          minMax range = {})
    {
        Q_ASSERT(settingsPanel);
        Q_ASSERT(settingsPanel->settingsArea());

        QWidget *parent = settingsPanel->settingsArea();
        auto *layout = qobject_cast<QFormLayout *>(parent->layout());

        Q_ASSERT(layout);

        auto *slider = new QSlider(Qt::Horizontal, parent);
        slider->setRange(static_cast<int>(range.min * sliderScale),
                         static_cast<int>(range.max * sliderScale));
        slider->setTracking(true);

        QObject::connect(slider, &QSlider::valueChanged, parent, [setter](int value)
                         { setter(value / sliderScaleF); });
        QObject::connect(settingsPanel, &SettingsPanel::refImageChanged, slider,
                         [slider, getter](const ReferenceImageSP &refImage)
                         { if (refImage) { slider->setValue(getter() * sliderScale); } });

        if (settingsPanel->referenceImage())
        {
            slider->setValue(getter() * sliderScale);
        }

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

SettingsPanel::SettingsPanel(ReferenceWindow *refWindow, QWidget *parent)
    : QWidget(parent, {})
{
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    new QGridLayout(this);

    buildInterface();
    setRefWindow(refWindow);
}

void SettingsPanel::setRefWindow(ReferenceWindow *refWindow)
{
    if (refWindow == m_refWindow)
    {
        return;
    }

    if (m_refWindow)
    {
        QObject::disconnect(m_refWindow, nullptr, this, nullptr);
    }

    m_refWindow = refWindow;

    if (refWindow)
    {
        setReferenceImage(m_refWindow->activeImage());
        QObject::connect(refWindow, &ReferenceWindow::activeImageChanged,
                         this, &SettingsPanel::setReferenceImage);
    }
    else
    {
        setReferenceImage(nullptr);
    }
}

void SettingsPanel::setReferenceImage(const ReferenceImageSP &image)
{
    if (m_refImage != image)
    {
        m_refImage = image;

        refreshInterface();
    }
    emit refImageChanged(m_refImage);
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

void SettingsPanel::initNoRefWidget()
{
    if (m_noRefWidget)
    {
        return;
    }

    auto *label = new QLabel("No Image", this);
    const int fontSize = 18;
    label->setAlignment(Qt::AlignCenter);
    label->setFont(QFont(label->font().families(), fontSize));
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_noRefWidget = label;

    m_toolBar->setEnabled(false);
}

void SettingsPanel::initSettingsArea()
{
    if (m_settingsArea)
    {
        return;
    }

    auto *scrollArea = new QScrollArea(this);
    auto *layout = new QFormLayout(scrollArea);

    m_settingsArea = scrollArea;

    createSlider(this, "Opacity:", [this]()
                 { return m_refImage->opacity(); }, [this](qreal value)
                 { m_refImage->setOpacity(value); });

    createSlider(this, "Saturation:", [this]()
                 { return m_refImage->saturation(); }, [this](qreal value)
                 { m_refImage->setSaturation(value); });
}

void SettingsPanel::buildInterface()
{
    Q_ASSERT(!m_toolBar && !m_settingsArea && !m_noRefWidget);

    auto *gridLayout = qobject_cast<QGridLayout *>(layout());
    Q_ASSERT(gridLayout);

    m_toolBar = createToolBar(*this);
    gridLayout->addWidget(m_toolBar, 0, 0, Qt::AlignTop);

    initSettingsArea();
    gridLayout->addWidget(m_settingsArea, 1, 0);

    initNoRefWidget();
    gridLayout->addWidget(m_noRefWidget, 1, 0);

    refreshInterface();
}

void SettingsPanel::refreshInterface()
{
    if (!m_refImage)
    {
        setWindowTitle("Settings");
        m_toolBar->setEnabled(false);

        m_settingsArea->hide();
        m_noRefWidget->show();
    }
    else
    {
        setWindowTitle(m_refImage->name().isEmpty() ? "Settings" : m_refImage->name() + " Settings");

        m_toolBar->setEnabled(true);

        m_settingsArea->show();
        m_noRefWidget->hide();
    }
}
