#include "preferences_window.h"

#include <QtGui/QCloseEvent>
#include <QtGui/QScreen>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStackedLayout>
#include <QtWidgets/QVBoxLayout>

#include "../app.h"
#include "../preferences.h"

#include "main_toolbar.h"

namespace
{
    using PrefLayoutType = QVBoxLayout;

    enum class PrefSubType
    {
        None,
        Percentage,
    };

    // Creates widgets for preferences.
    class PrefWidgetMaker
    {
    private:
        PrefLayoutType *m_layout;
        Preferences *m_prefs;

        Preferences::Keys m_key = Preferences::InvalidPreference;
        QString m_name;
        QString m_description;
        QMetaType::Type m_type = QMetaType::Type::Void;
        PrefSubType m_subType = PrefSubType::None;

    protected:
        QWidget *createWidgetBool();
        QWidget *createWidgetFloat();
        QWidget *createWidgetInt();

        QWidget *parentWidget() { return m_layout ? m_layout->parentWidget() : nullptr; }

    public:
        explicit PrefWidgetMaker(PrefLayoutType *layout, Preferences *prefs);

        QWidget *createWidget(Preferences::Keys key, PrefSubType subType = PrefSubType::None);
    };

    QWidget *PrefWidgetMaker::createWidgetBool()
    {
        auto *checkBox = new QCheckBox(m_name, parentWidget());
        checkBox->setChecked(m_prefs->getBool(m_key));
        checkBox->setToolTip(m_description);

        const auto key = m_key;
        auto *prefs = m_prefs;

        QObject::connect(checkBox, &QCheckBox::stateChanged, parentWidget(),
                         [=]() { prefs->setBool(key, checkBox->isChecked()); });

        m_layout->addWidget(checkBox);
        return checkBox;
    }

    QWidget *PrefWidgetMaker::createWidgetFloat()
    {
        const PrefFloatRange range = Preferences::getFloatRange(m_key);

        QScopedPointer<QHBoxLayout> hbox(new QHBoxLayout());

        auto *label = new QLabel(m_name + ":", parentWidget());
        label->setToolTip(m_description);

        auto *spinBox = new QDoubleSpinBox(parentWidget());
        spinBox->setSingleStep(range.size() / 100.);
        spinBox->setRange(range.min, range.max);
        spinBox->setToolTip(m_description);
        spinBox->setValue(m_prefs->getFloat(m_key));

        hbox->addWidget(label);
        hbox->addWidget(spinBox);

        const auto key = m_key;
        auto *prefs = m_prefs;

        QObject::connect(spinBox, &QDoubleSpinBox::valueChanged, parentWidget(),
                         [=](double d) { prefs->setFloat(key, d); });

        m_layout->addLayout(hbox.take());
        return spinBox;
    }

    QWidget *PrefWidgetMaker::createWidgetInt()
    {
        const PrefIntRange range = Preferences::getIntRange(m_key);
        QScopedPointer<QHBoxLayout> hbox(new QHBoxLayout());

        auto *label = new QLabel(m_name + ":", parentWidget());
        label->setToolTip(m_description);

        auto *spinBox = new QSpinBox(parentWidget());
        spinBox->setRange(range.min, range.max);
        spinBox->setToolTip(m_description);
        spinBox->setValue(m_prefs->getInt(m_key));

        hbox->addWidget(label);
        hbox->addWidget(spinBox);

        const auto key = m_key;
        auto *prefs = m_prefs;

        QObject::connect(spinBox, &QSpinBox::valueChanged, parentWidget(), [=](int i) { prefs->setInt(key, i); });

        m_layout->addLayout(hbox.take());

        return spinBox;
    }

    PrefWidgetMaker::PrefWidgetMaker(PrefLayoutType *layout, Preferences *prefs)
        : m_layout(layout),
          m_prefs(prefs)
    {
        if (!layout)
        {
            qFatal() << "PrefWidgetMaker constructor: layout cannot be null";
        }
    }

    QWidget *PrefWidgetMaker::createWidget(Preferences::Keys key, PrefSubType subType)
    {
        m_key = key;
        m_name = Preferences::getDisplayName(key);
        m_description = Preferences::getDescription(key);
        m_type = Preferences::getType(key);
        m_subType = subType;

        switch (m_type)
        {
        case QMetaType::Bool:
            return createWidgetBool();

        case QMetaType::Float:
        case QMetaType::Double:
            return createWidgetFloat();

        case QMetaType::Int:
            return createWidgetInt();

        case QMetaType::UnknownType:
        case QMetaType::Void:
            qCritical() << "Unable to find preference key" << key;
        default:
            return nullptr;
        }
    }

    void deleteUI(QWidget *widget)
    {
        for (auto *obj : widget->children())
        {
            if (obj->isWidgetType())
            {
                obj->deleteLater();
            }
        }
        delete widget->layout();
    }

} // namespace

PreferencesWindow::PreferencesWindow(QWidget *parent)
    : PreferencesWindow(nullptr, parent)
{
}

PreferencesWindow::PreferencesWindow(Preferences *prefs, QWidget *parent)
    : QWidget(parent),
      m_prefs(prefs)
{
    setWindowFlag(Qt::Dialog);

    if (m_prefs == nullptr)
    {
        m_prefs = appPrefs()->duplicate(this);
    }

    App *app = App::ghostRefInstance();

    // Position the window above the main toolbar
    if (const MainToolbar *mainToolbar = app->mainToolbar(); isWindow() && mainToolbar)
    {
        move(mainToolbar->pos());
    }

    buildUI();
}

void PreferencesWindow::setPrefs(Preferences *prefs)
{
    if (m_prefs && m_prefs->parent() == this)
    {
        m_prefs->deleteLater();
    }
    m_prefs = prefs;
}

QSize PreferencesWindow::sizeHint() const
{
    static const QSize fallbackSize = {360, 480};
    if (screen())
    {
        const QSize screenSize = screen()->size();
        return {screenSize.width() / 4, screenSize.height() / 2};
    }
    return fallbackSize;
}

void PreferencesWindow::savePreferences()
{
    App *app = App::ghostRefInstance();
    app->setPreferences(m_prefs);
    m_prefs->saveToDisk();
}

void PreferencesWindow::restoreDefaults()
{
    const int currentPage = m_pageList->currentRow();

    setPrefs(new Preferences(this));
    deleteUI(this);
    buildUI();

    m_pageList->setCurrentRow(currentPage);
}

void PreferencesWindow::closeEvent(QCloseEvent *event)
{
    if (!m_prefs->checkAllEqual(App::ghostRefInstance()->preferences()))
    {
        QMessageBox msgbox(QMessageBox::Question, "Save Changes", "Save changes to preferences?",
                           QMessageBox::Save | QMessageBox::Discard, this);
        switch (msgbox.exec())
        {
        case QMessageBox::Save:
            savePreferences();
        default:
            break;
        }
    }
    event->accept();
}

void PreferencesWindow::buildUI()
{
    if (layout() != nullptr)
    {
        qWarning() << "PreferencesWindow::buildUI: UI layout already initalized.";
        return;
    }

    auto *gridLayout = new QGridLayout(this);
    gridLayout->setRowStretch(1, 1);
    gridLayout->setColumnStretch(1, 1);

    m_pageList = new QListWidget(this);
    {
        const int fontSizePt = 12;
        const int widthPx = 128;

        m_pageList->addItem("General");
        m_pageList->addItem("Advanced");
        m_pageList->setCurrentRow(0);
        m_pageList->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        m_pageList->setMaximumWidth(widthPx);

        QFont font = m_pageList->font();
        font.setPointSize(fontSizePt);
        m_pageList->setFont(font);

        gridLayout->addWidget(m_pageList, 0, 0);
    }

    auto *pageFrame = new QFrame(this);
    auto *pageStack = new QStackedLayout(pageFrame);
    pageFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    gridLayout->addWidget(pageFrame, 0, 1, 2, 1);

    // Restore Defaults/Ok/Cancel buttons
    auto *buttons = new QWidget(this);
    {
        auto *layout = new QHBoxLayout(buttons);

        auto *resetBtn = new QPushButton("Restore Defaults");
        QObject::connect(resetBtn, &QPushButton::clicked, this, &PreferencesWindow::restoreDefaults);
        layout->addWidget(resetBtn);

        layout->addStretch();

        auto *acceptBtn = new QPushButton("Ok", buttons);
        QObject::connect(acceptBtn, &QPushButton::clicked, this, &PreferencesWindow::saveAndClose);
        layout->addWidget(acceptBtn);

        auto *cancelBtn = new QPushButton("Cancel", buttons);
        cancelBtn->setShortcut(QKeySequence::Cancel);
        QObject::connect(cancelBtn, &QPushButton::clicked, this, &PreferencesWindow::close);
        layout->addWidget(cancelBtn);

        gridLayout->addWidget(buttons, 2, 0, 1, 2);
    }

    // General Page
    auto *general = new QWidget(pageFrame);
    pageStack->addWidget(general);
    {
        auto *layout = new PrefLayoutType(general);
        PrefWidgetMaker widgetMaker(layout, m_prefs);

        widgetMaker.createWidget(Preferences::AllowInternet);
        widgetMaker.createWidget(Preferences::AskSaveBeforeClosing);
        widgetMaker.createWidget(Preferences::AnimateToolbarCollapse);
        widgetMaker.createWidget(Preferences::GhostModeOpacity);
        layout->addStretch();
    }

    // Advanced Page
    auto *advanced = new QWidget(pageFrame);
    pageStack->addWidget(advanced);
    {
        auto *layout = new PrefLayoutType(advanced);
        PrefWidgetMaker widgetMaker(layout, m_prefs);

        widgetMaker.createWidget(Preferences::LocalFilesLink);
        widgetMaker.createWidget(Preferences::LocalFilesStoreMaxMB);
        layout->addStretch();
    }

    QObject::connect(m_pageList, &QListWidget::currentRowChanged, pageStack, &QStackedLayout::setCurrentIndex);
}

void PreferencesWindow::saveAndClose()
{
    savePreferences();
    close();
}