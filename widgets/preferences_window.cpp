#include "preferences_window.h"

#include <QtGui/QCloseEvent>
#include <QtGui/QScreen>
#include <QtGui/QShortcut>

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
#include "../global_hotkeys.h"
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

    class HotkeyButton : public QPushButton
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(HotkeyButton)
    private:
        Preferences *m_prefs;
        QString m_hotkeyName;
        bool m_isGlobalHotkey;
        QKeyCombination m_keys;
        bool m_globalHotkeysEnabled = false;

    public:
        HotkeyButton(QString hotkey, Preferences *prefs, bool isGlobal, QWidget *parent = nullptr)
            : QPushButton(parent),
              m_prefs(prefs),
              m_hotkeyName(std::move(hotkey)),
              m_isGlobalHotkey(isGlobal)
        {
            setAutoRepeat(false);
            setCheckable(true);
            setFocusPolicy(Qt::ClickFocus);

            resetKeys();

            QObject::connect(this, &HotkeyButton::toggled, this, &HotkeyButton::onClicked);
        }
        ~HotkeyButton() override = default;

        bool eventFilter([[maybe_unused]] QObject *watched, QEvent *event) override
        {
            // Block shortcuts whilst this button is down
            if (event->type() == QEvent::ShortcutOverride && isChecked())
            {
                event->accept();
            }
            return false;
        }

        void resetKeys() { setKeys(hotkeyMap()[m_hotkeyName][0]); }

        void keyPressEvent(QKeyEvent *event) override
        {
            if (isChecked())
            {
                event->accept();

                if (event->isAutoRepeat()) return;

                if (event->key() != Qt::Key_Escape)
                {
                    setKeys(isModifier(event) ? QKeyCombination::fromCombined(event->modifiers())
                                              : event->keyCombination());
                }
            }
            else
            {
                QPushButton::keyPressEvent(event);
            }
        }

        void keyReleaseEvent(QKeyEvent *event) override
        {
            if (isChecked())
            {
                event->accept();

                if (event->isAutoRepeat()) return;

                if (event->key() == Qt::Key_Escape)
                {
                    cancel();
                }
                else if (!isModifier(event))
                {
                    setKeys(event->keyCombination());
                    accept();
                }
            }
            else
            {
                QPushButton::keyReleaseEvent(event);
            }
        }

    private:
        Preferences::HotkeyMap &hotkeyMap() const
        {
            return m_isGlobalHotkey ? m_prefs->globalHotkeys() : m_prefs->hotkeys();
        }

        void startInput()
        {
            setFocus();
            grabKeyboard();

            // Disable global hotkeys during input
            GlobalHotkeys *globalHotkeys = App::ghostRefInstance()->globalHotkeys();
            m_globalHotkeysEnabled = globalHotkeys->isEnabled();
            globalHotkeys->setEnabled(false);

            if (window()) window()->installEventFilter(this);
        }

        void accept() { finish(true); }
        void cancel() { finish(false); }

        void finish(bool accepted)
        {
            setChecked(false);
            releaseKeyboard();

            // Reenable global hotkeys if they were originally enabled
            App::ghostRefInstance()->globalHotkeys()->setEnabled(m_globalHotkeysEnabled);

            if (window()) window()->removeEventFilter(this);

            if (accepted)
            {
                hotkeyMap()[m_hotkeyName] = m_keys;
            }
        }

        void onClicked(bool checked)
        {
            if (checked) startInput();
        }

        void setKeys(const QKeyCombination &keys)
        {
            m_keys = keys;
            const QString newText = QKeySequence(m_keys).toString(QKeySequence::NativeText);
            setText(newText.isEmpty() ? "None" : newText);
        }

        static bool isModifier(const QKeyEvent *event)
        {
            switch (event->key())
            {
            case Qt::Key_Control:
            case Qt::Key_Shift:
            case Qt::Key_Alt:
            case Qt::Key_AltGr:
            case Qt::Key_Meta:
                return true;
            default:
                return false;
            }
        }
    };

    // Creates widgets for preferences.
    class PrefWidgetMaker
    {
    private:
        QBoxLayout *m_layout;
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
        explicit PrefWidgetMaker(QBoxLayout *layout, Preferences *prefs);

        QWidget *createWidget(Preferences::Keys key, PrefSubType subType = PrefSubType::None);
        QWidget *createHotkeyWidget(const QString &hotkeyName, bool globalHotkey = false);
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

    PrefWidgetMaker::PrefWidgetMaker(QBoxLayout *layout, Preferences *prefs)
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

    QWidget *PrefWidgetMaker::createHotkeyWidget(const QString &hotkeyName, bool globalHotkey)
    {
        if (!globalHotkey)
        {
            qCritical() << "Only global hotkeys are currently implemented";
            return nullptr;
        }
        const Preferences::HotkeyMap &hotkeys = m_prefs->globalHotkeys();

        QScopedPointer<QHBoxLayout> hbox(new QHBoxLayout());
        hbox->setSpacing(0);

        auto *hotkeyBtn = new HotkeyButton(hotkeyName, m_prefs, globalHotkey, parentWidget());

        auto *resetBtn = new QPushButton(parentWidget());
        resetBtn->setIcon(resetBtn->style()->standardIcon(QStyle::SP_BrowserReload));
        resetBtn->setToolTip("Restore default");
        resetBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

        Preferences *prefs = m_prefs;
        QObject::connect(resetBtn, &QPushButton::clicked, hotkeyBtn, [=]() {
            prefs->resetHotkey(hotkeyName, globalHotkey);
            hotkeyBtn->resetKeys();
        });

        auto *clearBtn = new QPushButton(parentWidget());
        clearBtn->setIcon(clearBtn->style()->standardIcon(QStyle::SP_DialogCloseButton));
        clearBtn->setToolTip("Clear");
        clearBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

        QObject::connect(clearBtn, &QPushButton::clicked, hotkeyBtn, [=]() {
            (globalHotkey ? prefs->globalHotkeys() : prefs->hotkeys())[hotkeyName] = {};
            hotkeyBtn->resetKeys();
        });

        auto *label = new QLabel(hotkeyName, parentWidget());

        hbox->addWidget(label);
        hbox->addWidget(hotkeyBtn);
        hbox->addWidget(resetBtn);
        hbox->addWidget(clearBtn);

        m_layout->addLayout(hbox.take());
        return hotkeyBtn;
    }

    QWidget *createOverrideKeyWidget(PrefLayoutType *layout, Preferences *prefs)
    {
        auto *widget = new QWidget(layout->parentWidget());
        auto *widgetLayout = new QHBoxLayout(widget);
        widgetLayout->setContentsMargins(0, 0, 0, 0);

        widget->setToolTip("Ghost Mode is deactivated whilst this key combination is held.");

        widgetLayout->addWidget(new QLabel("Un-ghost keys:", widget));
        widgetLayout->addStretch();

        PrefWidgetMaker widgetMaker(widgetLayout, prefs);

        for (auto k : {Preferences::OverrideKeyAlt, Preferences::OverrideKeyCtrl, Preferences::OverrideKeyShift})
        {
            widgetMaker.createWidget(k);
        }

        layout->addWidget(widget);

        return widget;
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
        m_pageList->addItem("Global Hotkeys");

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

        // auto *closeShortcut =
        //     new QShortcut(QKeySequence(Qt::Key_Escape), this, [this]() { close(); }, Qt::WidgetWithChildrenShortcut);

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
        createOverrideKeyWidget(layout, m_prefs);

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
        widgetMaker.createWidget(Preferences::UndoMaxSteps);
        layout->addStretch();
    }

    // Global Hotkeys Page
    auto *globalHotkeys = new QWidget(pageFrame);
    pageStack->addWidget(globalHotkeys);
    {
        auto *layout = new PrefLayoutType(globalHotkeys);
        PrefWidgetMaker widgetMaker(layout, m_prefs);

        widgetMaker.createWidget(Preferences::GlobalHotkeysEnabled);

        for (const QString &name : Preferences::defaultGlobalHotkeys().keys())
        {
            widgetMaker.createHotkeyWidget(name, true);
        }

        layout->addStretch();
    }

    QObject::connect(m_pageList, &QListWidget::currentRowChanged, pageStack, &QStackedLayout::setCurrentIndex);
}

void PreferencesWindow::saveAndClose()
{
    savePreferences();
    close();
}

#include "preferences_window.moc"