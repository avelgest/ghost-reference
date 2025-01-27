#include "settings_panel.h"

#include <initializer_list>
#include <utility>

#include <QtCore/Qt>

#include <QtGui/QMouseEvent>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QSlider>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>

#include "../app.h"
#include "../reference_image.h"
#include "../tools/color_picker.h"
#include "../tools/extract_tool.h"
#include "../undo_stack.h"

#include "back_window.h"
#include "back_window_actions.h"
#include "reference_window.h"

namespace
{
    const Qt::WindowFlags defaultWindowFlags = Qt::Tool;
    const QSize defaultSizeHint(296, 420);

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
                         [comboBox, getter](const ReferenceImageSP &refImage) {
                             if (refImage)
                             {
                                 const int idx = comboBox->findData(static_cast<int>(getter()));
                                 comboBox->setCurrentIndex(idx);
                             }
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
                         [=](const ReferenceImageSP &refImage) {
                             if (refImage) checkBox->setChecked(getter());
                         });

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

        QObject::connect(settingsPanel, &SettingsPanel::refImageChanged, lineEdit,
                         [=](const ReferenceImageSP &refImage) {
                             if (refImage) lineEdit->setText(refImage->name());
                         });

        layout->addRow(lineEdit);

        return lineEdit;
    }

    void createCropSettings(SettingsPanel *settingsPanel, QFormLayout *layout)
    {
        auto *const parent = settingsPanel;
        const int minInputWidth = 10;

        auto *hbox = new QHBoxLayout();
        hbox->setSpacing(0);

        auto *xInput = new QLineEdit(parent);
        auto *yInput = new QLineEdit(parent);
        auto *widthInput = new QLineEdit(parent);
        auto *heightInput = new QLineEdit(parent);

        xInput->setToolTip("Left");
        yInput->setToolTip("Top");
        widthInput->setToolTip("Width");
        heightInput->setToolTip("Height");

        auto updateCrop = [=]() {
            const ReferenceImageSP &refImage = settingsPanel->referenceImage();
            if (refImage.isNull())
            {
                return;
            }
            const QRect oldCrop = refImage->crop();

            const QRect newCrop(xInput->text().toInt(), yInput->text().toInt(), widthInput->text().toInt(),
                                heightInput->text().toInt());
            if (newCrop != oldCrop && !newCrop.isEmpty())
            {
                UndoStack::get()->pushWindowAndRefItem(settingsPanel->refWindow(), refImage);
                settingsPanel->refWindow()->setCrop(newCrop);
            }
        };

        for (auto *input : {xInput, yInput, widthInput, heightInput})
        {
            input->setMinimumWidth(minInputWidth);
            QObject::connect(input, &QLineEdit::editingFinished, settingsPanel, updateCrop);
            hbox->addWidget(input);
        }

        auto updateInputs = [=]() {
            const ReferenceImageSP &refImage = settingsPanel->referenceImage();
            const QRect crop = refImage.isNull() ? QRect() : refImage->crop();
            xInput->setText(QString::number(crop.left()));
            yInput->setText(QString::number(crop.top()));
            widthInput->setText(QString::number(crop.width()));
            heightInput->setText(QString::number(crop.height()));
        };

        QObject::connect(settingsPanel, &SettingsPanel::refImageChanged, parent, [=](const ReferenceImageSP &refImage) {
            if (refImage)
            {
                // This should be disconnected by SettingsPanel::setReferenceImage
                QObject::connect(refImage.get(), &ReferenceImage::cropChanged, settingsPanel, updateInputs);
            }
            updateInputs();
        });

        auto *resetBtn = new QPushButton(parent->style()->standardIcon(QStyle::SP_BrowserReload), "", parent);
        resetBtn->setToolTip("Reset crop");
        QObject::connect(resetBtn, &QPushButton::clicked, settingsPanel, [=]() {
            if (const ReferenceImageSP &refImage = settingsPanel->referenceImage(); refImage)
            {
                settingsPanel->refWindow()->setCrop(refImage->baseImage().rect());
            }
        });
        hbox->addWidget(resetBtn);

        layout->addRow("Crop:", hbox);
    }

    void createLinkSettings(SettingsPanel *settingsPanel, QFormLayout *layout)
    {
        auto *frame = new QGroupBox("File", settingsPanel);
        auto *frameLayout = new QFormLayout(frame);

        auto *filepathBox = new QLineEdit(frame);
        filepathBox->setReadOnly(true);
        filepathBox->setObjectName("filepath-text");

        frameLayout->addRow(filepathBox);

        auto *checkBox = createCheckbox(
            settingsPanel, frameLayout, "Store as Link",
            [=]() { return settingsPanel->referenceImage()->savedAsLink(); },
            [=](bool value) { settingsPanel->referenceImage()->setSavedAsLink(value); });

        checkBox->setToolTip("Only store a link when saving the session rather than storing a copy of this file.");

        QObject::connect(settingsPanel, &SettingsPanel::refImageChanged, filepathBox,
                         [=](const ReferenceImageSP &image) {
                             frame->setVisible(image && image->isLocalFile());
                             if (image)
                             {
                                 filepathBox->setText(image->filepath());
                                 checkBox->setChecked(image->savedAsLink());
                             }
                         });
        layout->addRow(frame);
    }

    QPushButton *createFlipButton(SettingsPanel *parent, Qt::Orientation orientation)
    {
        const QSize buttonSize(42, 42);
        const QSize iconSize(36, 36);
        const bool isVertical = orientation == Qt::Vertical;
        auto *flipButton = new QPushButton(parent);

        const char *iconPath = App::isDarkMode() ? (isVertical ? ":/flip_btn_v_dark.png" : ":/flip_btn_h_dark.png")
                                                 : (isVertical ? ":/flip_btn_v.png" : ":/flip_btn_h.png");

        flipButton->setIcon(QIcon(iconPath));
        flipButton->setIconSize(iconSize);
        flipButton->setFixedSize(buttonSize);
        flipButton->setObjectName(isVertical ? "flip-btn-v" : "flip-btn-h");

        QObject::connect(flipButton, &QPushButton::clicked, parent,
                         isVertical ? &SettingsPanel::flipImageVertically : &SettingsPanel::flipImageHorizontally);
        return flipButton;
    }

    void createFlipSettings(SettingsPanel *settingsPanel, QFormLayout *layout)
    {
        auto *hBox = new QHBoxLayout();

        for (const auto &orientation : {Qt::Vertical, Qt::Horizontal})
        {
            auto *flipButton = createFlipButton(settingsPanel, orientation);
            hBox->addWidget(flipButton);
        }

        layout->addRow("Flip:", hBox);
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
                if (refWin && refWin->isAncestorOf(widget))
                {
                    return refWin;
                }
            }
        }
        return nullptr;
    }

} // namespace

SettingsPanel::SettingsPanel(ReferenceWindow *refWindow, QWidget *parent)
    : QFrame(parent, {})
{
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    // Graphics effects can cause issues when the panel is moved outside of the screen (see QTBUG-58602)
    // so disable for now.
    // setGraphicsEffect(createShadowEffect(this));

    buildUI();
    setRefWindow(refWindow);

    // Connecting directly to onAppFocusChanged may sometimes cause an assert to fail in Qt so connect
    // to a lambda instead.
    QObject::connect(App::ghostRefInstance(), &App::focusObjectChanged, this,
                     [this](QObject *obj) { onAppFocusChanged(obj); });
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
        QObject::connect(refWindow, &ReferenceWindow::activeImageChanged, this, &SettingsPanel::setReferenceImage);
        QObject::connect(refWindow, &ReferenceWindow::destroyed, this, [this]() { setRefWindow(nullptr); });
        QObject::connect(refWindow, &ReferenceWindow::ghostRefHiddenChanged, this,
                         [this](bool hidden) { emit refWindowHiddenChanged(hidden); });
        emit refWindowHiddenChanged(m_refWindow->ghostRefHidden());
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

void SettingsPanel::duplicateActiveRef() const
{
    if (m_refWindow)
    {
        m_refWindow->duplicateActive();
    }
}

void SettingsPanel::flipImageHorizontally() const
{
    m_refImage->setFlipHorizontal(!m_refImage->flipHorizontal());
}

void SettingsPanel::flipImageVertically() const
{
    m_refImage->setFlipVertical(!m_refImage->flipVertical());
}

void SettingsPanel::removeRefItemFromWindow()
{
    if (!m_refWindow || !m_refImage) return;

    App::ghostRefInstance()->undoStack()->pushWindowAndRefItem(m_refWindow, m_refImage);

    m_refWindow->removeReference(m_refImage);

    if (m_refWindow->referenceImages().isEmpty())
    {
        m_refWindow->close();
    }
}

void SettingsPanel::toggleRefWindowHidden() const
{
    if (m_refWindow)
    {
        UndoStack::get()->pushRefWindow(m_refWindow, false);
        m_refWindow->setGhostRefHidden(!m_refWindow->ghostRefHidden());
    }
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

    // ReferenceWindow Settings
    {
        auto *groupBox = new QGroupBox("Window", m_settingsArea);
        auto *groupLayout = new QFormLayout(groupBox);

        createSlider(
            this, groupLayout, "Opacity:", [this]() { return m_refWindow->opacity(); },
            [this](qreal value) { m_refWindow->setOpacity(value); });

        using enum ReferenceWindow::TabFit;
        createComboBox(
            this, groupLayout, "Fit Tabs to:",
            {
                {"Width", static_cast<int>(FitToWidth)},
                {"Height", static_cast<int>(FitToHeight)},
                {"None", static_cast<int>(NoFit)},
            },
            [this]() { return static_cast<int>(m_refWindow->tabFit()); },
            [this](QVariant value) { m_refWindow->setTabFit(variantToEnum<ReferenceWindow::TabFit>(value)); });

        layout->addRow(groupBox);
    }

    // ReferenceImage settings
    {
        auto *groupBox = new QGroupBox("Image", m_settingsArea);
        auto *groupLayout = new QFormLayout(groupBox);

        createSlider(
            this, groupLayout, "Saturation:", [this]() { return m_refImage->saturation(); },
            [this](qreal value) { m_refImage->setSaturation(value); });

        createCheckbox(
            this, groupLayout, "Smooth Filtering", [this]() { return m_refImage->smoothFiltering(); },
            [this](bool value) { m_refImage->setSmoothFiltering(value); });

        createCropSettings(this, groupLayout);

        createFlipSettings(this, groupLayout);

        layout->addRow(groupBox);
    }

    createLinkSettings(this, layout);
}

void SettingsPanel::initToolbar()
{
    Q_ASSERT(App::ghostRefInstance()->backWindow());
    BackWindowActions *windowActions = App::ghostRefInstance()->backWindow()->backWindowActions();

    m_toolBar = new QToolBar(this);
    m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    SettingsPanel *settingsPanel = this;

    QAction *action = nullptr;

    // Hide Window
    action = m_toolBar->addAction(QIcon(":/visible.png"), "Hide Window");
    QObject::connect(action, &QAction::triggered, settingsPanel, &SettingsPanel::toggleRefWindowHidden);
    QObject::connect(settingsPanel, &SettingsPanel::refWindowHiddenChanged, action, [action](bool hidden) {
        action->setIcon(hidden ? QIcon(":/hidden.png") : QIcon(":/visible.png"));
    });

    // Reload
    action = m_toolBar->addAction(m_toolBar->style()->standardIcon(QStyle::SP_BrowserReload), "Reload");
    action->setToolTip("Reload - Reload the reference from disk.");
    QObject::connect(action, &QAction::triggered, settingsPanel, [=]() { settingsPanel->referenceImage()->reload(); });
    QObject::connect(settingsPanel, &SettingsPanel::refImageChanged, action,
                     [=](const ReferenceImageSP &image) { action->setEnabled(image && image->isLocalFile()); });

    // Copy to Clipboard
    action = m_toolBar->addAction(QIcon::fromTheme(QIcon::ThemeIcon::EditCopy), "Copy to Clipboard");
    QObject::connect(action, &QAction::triggered, settingsPanel, &SettingsPanel::copyImageToClipboard);

    // Duplicate
    action = m_toolBar->addAction(QIcon(":/duplicate.png"), "Duplicate");
    QObject::connect(action, &QAction::triggered, settingsPanel, &SettingsPanel::duplicateActiveRef);

    m_toolBar->addAction(&windowActions->colorPicker());

    m_toolBar->addAction(&windowActions->extractTool());

    // Delete Reference
    action = m_toolBar->addAction(m_toolBar->style()->standardIcon(QStyle::SP_DialogDiscardButton), "Delete Reference");
    QObject::connect(action, &QAction::triggered, settingsPanel, &SettingsPanel::removeRefItemFromWindow);
}

void SettingsPanel::buildUI()
{
    Q_ASSERT(!m_titleBar && !m_toolBar);

    auto *gridLayout = new QGridLayout(this);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(0);

    m_titleBar = new TitleBar(this);
    gridLayout->addWidget(m_titleBar, 0, 0, Qt::AlignTop);

    initToolbar();
    Q_ASSERT(m_toolBar != nullptr);
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

void SettingsPanel::copyImageToClipboard() const
{
    if (refWindow())
    {
        refWindow()->copyActiveToClipboard();
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
