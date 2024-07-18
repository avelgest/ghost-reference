#include "settings_panel.h"

#include <initializer_list>
#include <utility>

#include <QtCore/Qt>

#include <QtGui/QMouseEvent>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QSlider>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>

#include "../app.h"
#include "../color_picker.h"
#include "../reference_image.h"
#include "reference_window.h"

namespace
{
    const Qt::WindowFlags defaultWindowFlags = Qt::Tool;
    const QSize defaultSizeHint(256, 376);

    const int sliderScale = 100;
    const qreal sliderScaleF = static_cast<qreal>(sliderScale);

    struct minMax
    {
        qreal min = 0.;
        qreal max = 1.;
    };

    class TitleBar : public QWidget
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(TitleBar)
    private:
        QLabel *m_TitleLabel;
        QPointF m_lastMousePos;
        QString m_title;

    public:
        explicit TitleBar(SettingsPanel *settingsPanel);
        ~TitleBar() override = default;

        void setTitle(const QString &title);

        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void resizeEvent(QResizeEvent *event) override;
    };

    TitleBar::TitleBar(SettingsPanel *settingsPanel)
        : QWidget(settingsPanel),
          m_TitleLabel(new QLabel(this))
    {
        const int buttonWidth = 48;
        const int barHeightMin = 32;
        const int marginLeft = 12;

        setCursor(Qt::SizeAllCursor);
        setObjectName("title-bar");
        setMinimumHeight(barHeightMin);

        auto *layout = new QHBoxLayout(this);

        m_TitleLabel->setObjectName("title-bar-title-label");
        m_TitleLabel->setTextFormat(Qt::PlainText);
        m_TitleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
        layout->addWidget(m_TitleLabel);
        layout->setStretchFactor(m_TitleLabel, 1);

        layout->setSpacing(0);
        layout->setContentsMargins(marginLeft, 0, 0, 0);

        auto *closeBtn = new QPushButton(style()->standardIcon(QStyle::SP_TitleBarCloseButton), "", this);
        closeBtn->setAttribute(Qt::WA_NoMousePropagation);
        closeBtn->setCursor({});
        closeBtn->setFlat(true);
        closeBtn->setMinimumWidth(buttonWidth);
        closeBtn->setObjectName("title-bar-close");
        closeBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        QObject::connect(closeBtn, &QPushButton::clicked, settingsPanel, &SettingsPanel::close);
        layout->addWidget(closeBtn);

        setTitle(settingsPanel->windowTitle());
        QObject::connect(settingsPanel, &SettingsPanel::windowTitleChanged, this, &TitleBar::setTitle);
    }

    void TitleBar::setTitle(const QString &title)
    {
        m_title = title;
        const QString text = fontMetrics().elidedText(m_title, Qt::ElideRight, m_TitleLabel->width());
        m_TitleLabel->setText(text);
    }

    void TitleBar::mousePressEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::LeftButton)
        {
            m_lastMousePos = event->globalPos();
        }
    }

    void TitleBar::mouseMoveEvent(QMouseEvent *event)
    {
        if (parentWidget() && event->buttons() == Qt::LeftButton)
        {
            const QPointF diff = event->globalPos() - m_lastMousePos;
            parentWidget()->move((parentWidget()->pos() + diff).toPoint());
            m_lastMousePos = event->globalPos();
        }
    }

    void TitleBar::resizeEvent(QResizeEvent *event)
    {
        QWidget::resizeEvent(event);
        setTitle(m_title);
    }

    template <typename Func1, typename Func2>
    QSlider *createSlider(SettingsPanel *settingsPanel, QFormLayout *layout, const QString &label, Func1 getter,
                          Func2 setter, minMax range = {})
    {
        Q_ASSERT(settingsPanel);
        Q_ASSERT(settingsPanel->settingsArea());

        QWidget *parent = settingsPanel->settingsArea();

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

    /*
    Creates a QComboBox and adds items as text/userData pairs given in options.
    getter should be a function of the form QVariant func() that returns the current value to select
    (by it's userData).
    setter should be a function of the form void func(QVariant& userData) that sets a property to the
    value indicated by userData.
    */
    template <typename Func1, typename Func2>
    QComboBox *createComboBox(SettingsPanel *settingsPanel, QFormLayout *layout, const QString &label,
                              std::initializer_list<std::pair<QString, QVariant>> options, Func1 getter, Func2 setter)
    {
        QWidget *parent = settingsPanel->settingsArea();
        Q_ASSERT(parent != nullptr);

        auto *comboBox = new QComboBox(parent);
        for (const auto &[text, data] : options)
        {
            comboBox->addItem(text, data);
        }

        if (settingsPanel->referenceImage())
        {
            comboBox->setCurrentIndex(comboBox->findData(getter()));
        }

        QObject::connect(comboBox, &QComboBox::currentIndexChanged,
                         [comboBox, setter](int index) { setter(comboBox->itemData(index)); });

        QObject::connect(settingsPanel, &SettingsPanel::refImageChanged, comboBox,
                         [comboBox, getter]([[maybe_unused]] const ReferenceImageSP &refImage) {
                             const int idx = comboBox->findData(static_cast<int>(getter()));
                             comboBox->setCurrentIndex(idx);
                         });

        layout->addRow(label, comboBox);
        return comboBox;
    }

    template <typename Func1, typename Func2>
    QCheckBox *createCheckbox(SettingsPanel *settingsPanel, QFormLayout *layout, const QString &label, Func1 getter,
                              Func2 setter)
    {
        auto *checkBox = new QCheckBox(settingsPanel->settingsArea());
        checkBox->setChecked(settingsPanel->referenceImage() ? getter() : false);
        checkBox->setText(label);

        QObject::connect(checkBox, &QCheckBox::toggled, [=]() { setter(checkBox->isChecked()); });
        QObject::connect(settingsPanel, &SettingsPanel::refImageChanged, checkBox,
                         [=]() { checkBox->setChecked(getter()); });

        layout->addRow("", checkBox);
        return checkBox;
    }

    // Creates a QLineEdit for displaying/editing the name of the active reference image
    QLineEdit *createRefNameInput(SettingsPanel *settingsPanel, QFormLayout *layout)
    {
        auto *lineEdit = new QLineEdit(settingsPanel->settingsArea());
        lineEdit->setObjectName("ref-name-input");
        QObject::connect(lineEdit, &QLineEdit::editingFinished, [=]() {
            settingsPanel->referenceImage()->setName(lineEdit->text());
            // The new name may be different from the submitted text so the new text of lineEdit
            // will be set by the onRefNameChanged slot.
        });

        QObject::connect(settingsPanel, &SettingsPanel::refImageChanged, lineEdit, [=]() {
            lineEdit->setText(settingsPanel->referenceImage() ? settingsPanel->referenceImage()->name() : "");
        });

        layout->addRow(lineEdit);

        return lineEdit;
    }

    QToolButton *createFlipButton(SettingsPanel &parent, Qt::Orientation orientation)
    {
        const char *iconPath = (orientation == Qt::Vertical) ? "resources/flip_btn_v.png"
                                                             : "resources/flip_btn_h.png";
        auto *flipButton = new QToolButton(&parent);
        flipButton->setAutoRaise(true);
        flipButton->setIcon(QIcon(iconPath));

        QObject::connect(flipButton, &QToolButton::clicked, &parent,
                         (orientation == Qt::Vertical) ? &SettingsPanel::flipImageVertically
                                                       : &SettingsPanel::flipImageHorizontally);
        return flipButton;
    }

    QToolBar *createToolBar(SettingsPanel *settingsPanel)
    {
        Q_ASSERT(settingsPanel != nullptr);

        auto *toolBar = new QToolBar(settingsPanel);
        QAction *action = nullptr;

        action = toolBar->addAction(QIcon("resources/flip_btn_h.png"), "Flip Horizontally");
        QObject::connect(action, &QAction::triggered, settingsPanel, &SettingsPanel::flipImageHorizontally);

        action = toolBar->addAction(QIcon("resources/flip_btn_v.png"), "Flip Vertically");
        QObject::connect(action, &QAction::triggered, settingsPanel, &SettingsPanel::flipImageVertically);

        action = toolBar->addAction(QIcon("resources/color_picker.png"), "Color Picker");
        QObject::connect(action, &QAction::triggered, settingsPanel,
                         [settingsPanel]() { Tool::activateTool<ColorPicker>(); });

        return toolBar;
    }

    template <typename E> E variantToEnum(QVariant &variant)
    {
        return variant.isValid() ? static_cast<E>(variant.toInt()) : E();
    }

    QGraphicsEffect *createShadowEffect(SettingsPanel *parent)
    {
        const float alpha = 0.4;
        const int blurRadius = 12;
        const int offset = 4;

        auto *effect = new QGraphicsDropShadowEffect(parent);
        QColor color = effect->color();
        color.setAlphaF(alpha);

        effect->setColor(color);
        effect->setOffset(offset, offset);
        effect->setBlurRadius(blurRadius);

        return effect;
    }

    ReferenceWindow *refWindowOf(QObject *obj)
    {
        QWidget *widget = qobject_cast<QWidget *>(obj);
        if (widget != nullptr)
        {
            for (ReferenceWindow *refWin : App::ghostRefInstance()->referenceWindows())
            {
                if (refWin->isAncestorOf(widget))
                {
                    return refWin;
                }
            }
        }
        return nullptr;
    }

} // namespace

SettingsPanel::SettingsPanel(ReferenceWindow *refWindow, QWidget *parent) : QFrame(parent, {})
{
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    // Graphics effects can cause issues when the panel is moved outside of the screen (see QTBUG-58602)
    // so disable for now.
    // setGraphicsEffect(createShadowEffect(this));

    buildUI();
    setRefWindow(refWindow);

    QObject::connect(App::ghostRefInstance(), &App::focusObjectChanged, this, &SettingsPanel::onAppFocusChanged);
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
        if (!m_refImage.isNull())
        {
            QObject::disconnect(m_refImage.get(), nullptr, this, nullptr);
        }

        m_refImage = image;

        if (!m_refImage.isNull())
        {
            QObject::connect(m_refImage.get(), &ReferenceImage::nameChanged, this, &SettingsPanel::onRefNameChanged);
        }

        refreshUI();
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
    Q_ASSERT(m_noRefWidget == nullptr);

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
    Q_ASSERT(m_settingsArea == nullptr);
    Q_ASSERT(m_settingsAreaScroll == nullptr);

    m_settingsArea = new QWidget(this);
    m_settingsArea->setObjectName("settings-area");

    m_settingsAreaScroll = new QScrollArea(this);
    m_settingsAreaScroll->setObjectName("settings-area-scroll");
    m_settingsAreaScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_settingsAreaScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_settingsAreaScroll->setSizeAdjustPolicy(QScrollArea::AdjustIgnored);
    m_settingsAreaScroll->setWidgetResizable(true);
    m_settingsAreaScroll->setWidget(m_settingsArea);

    auto *layout = new QFormLayout(m_settingsArea);

    createRefNameInput(this, layout);

    createSlider(
        this, layout, "Opacity:", [this]() { return m_refImage->opacity(); },
        [this](qreal value) { m_refImage->setOpacity(value); });

    createSlider(
        this, layout, "Saturation:", [this]() { return m_refImage->saturation(); },
        [this](qreal value) { m_refImage->setSaturation(value); });

    createCheckbox(
        this, layout, "Smooth Filtering", [this]() { return m_refImage->smoothFiltering(); },
        [this](bool value) { m_refImage->setSmoothFiltering(value); });

    using enum ReferenceWindow::TabFit;
    createComboBox(
        this, layout, "Fit Tabs to:",
        {
            {"Width", static_cast<int>(FitToWidth)},
            {"Height", static_cast<int>(FitToHeight)},
            {"None", static_cast<int>(NoFit)},
        },
        [this]() { return static_cast<int>(m_refWindow->tabFit()); },
        [this](QVariant value) { m_refWindow->setTabFit(variantToEnum<ReferenceWindow::TabFit>(value)); });
}

void SettingsPanel::buildUI()
{
    Q_ASSERT(!m_titleBar && !m_toolBar);

    auto *gridLayout = new QGridLayout(this);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(0);

    m_titleBar = new TitleBar(this);
    gridLayout->addWidget(m_titleBar, 0, 0, Qt::AlignTop);

    m_toolBar = createToolBar(this);
    gridLayout->addWidget(m_toolBar, 1, 0, Qt::AlignTop);

    initSettingsArea();
    gridLayout->addWidget(m_settingsAreaScroll, 2, 0);

    initNoRefWidget();
    gridLayout->addWidget(m_noRefWidget, 2, 0);

    refreshUI();
}

void SettingsPanel::refreshUI()
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
        setWindowTitle(m_refImage->name().isEmpty() ? "Settings" : m_refImage->name());

        m_toolBar->setEnabled(true);

        m_settingsArea->show();
        m_noRefWidget->hide();
    }
}

void SettingsPanel::onAppFocusChanged(QObject *focusObject)
{
    ReferenceWindow *refWindow = refWindowOf(focusObject);
    if (refWindow)
    {
        setRefWindow(refWindow);
    }
}

void SettingsPanel::onRefNameChanged(const QString &newName)
{
    setWindowTitle(newName);
    if (auto *nameInput = findChild<QLineEdit *>("ref-name-input"); nameInput)
    {
        nameInput->setText(newName);
    }
}

#include "settings_panel.moc"
