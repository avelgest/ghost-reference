#include "help_window.h"

#include <optional>

#include <QtWidgets/QStyle>

#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

#include "../app.h"
#include "../global_hotkeys.h"
#include "../preferences.h"

#include "back_window.h"
#include "back_window_actions.h"

namespace
{
    BackWindowActions *getBackWindowActions()
    {
        return App::ghostRefInstance()->backWindow()->backWindowActions();
    }

    bool isDebug()
    {
#ifdef QT_NO_DEBUG
        return false;
#else
        return true;
#endif // !QT_NO_DEBUG
    }

    QString versionStr()
    {
        auto base = "Version " % QString::number(GHOST_REF_VERSION_MAJOR) % "." %
                    QString::number(GHOST_REF_VERSION_MINOR) % "." % QString::number(GHOST_REF_VERSION_PATCH);

        return isDebug() ? (base % " " % "Debug") : QString(base);
    }

    QString shortcutString(const QKeySequence &shortcut, const char *fallback = "None")
    {
        QString shortcutStr = shortcut.toString(QKeySequence::NativeText);
        return shortcutStr.isEmpty() ? fallback : shortcutStr;
    }

    QWidget *hotkeyWidget(const QKeySequence &shortcut, QWidget *parent)
    {
        QString shortcutStr = shortcutString(shortcut);
        auto *label = new QLabel(shortcutStr, parent);
        label->setAlignment(Qt::AlignHCenter);
        return label;
    }

    QWidget *hotkeyWidget(const QAction &action, QWidget *parent)
    {
        return hotkeyWidget(action.shortcut(), parent);
    }

    void addGlobalHotkeyRow(QFormLayout *layout, GlobalHotkeys::BuiltIn builtIn)
    {
        layout->addRow(GlobalHotkeys::builtinName(builtIn),
                       hotkeyWidget(GlobalHotkeys::getKey(builtIn), layout->parentWidget()));
    }

    QIcon nextIcon()
    {
        return QIcon::hasThemeIcon(QIcon::ThemeIcon::GoNext) ? QIcon::fromTheme(QIcon::ThemeIcon::GoNext)
                                                             : App::style()->standardIcon(QStyle::SP_ArrowForward);
    }

    QIcon backIcon()
    {
        return QIcon::hasThemeIcon(QIcon::ThemeIcon::GoPrevious) ? QIcon::fromTheme(QIcon::ThemeIcon::GoPrevious)
                                                                 : App::style()->standardIcon(QStyle::SP_ArrowBack);
    }

    QString overrideKeysStr()
    {
        const Qt::KeyboardModifiers keys = appPrefs()->overrideKeys();
        if (keys == Qt::NoModifier) return "Not Set";

        QString str;
        if (keys & Qt::ControlModifier) str = "CTRL";
        if (keys & Qt::AltModifier) str += (str.isEmpty() ? "ALT" : " + ALT");
        if (keys & Qt::ShiftModifier) str += (str.isEmpty() ? "SHIFT" : " + SHIFT");
        return str;
    }

} // namespace

HelpWindow::HelpWindow(QWidget *parent)
    : QWidget(parent, Qt::Dialog)
{
    const QSize minSize(420, 560);

    const QMargins boxMargins(8, 0, 8, 8);

    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumSize(minSize);
    setWindowTitle("Help");

    auto *layout = new QVBoxLayout(this);

    // Header
    {
        const int iconExtent = 96;

        auto *header = new QWidget(this);
        auto *grid = new QGridLayout(header);

        auto *iconLabel = new QLabel(header);
        iconLabel->setPixmap(QIcon(":appicon.ico").pixmap(iconExtent));
        grid->addWidget(iconLabel, 0, 0, 3, 1);

        auto *titleLabel = new QLabel("Ghost Reference", header);
        titleLabel->setObjectName("help-title-label");
        grid->addWidget(titleLabel, 0, 1, Qt::AlignTop);

        auto *versionLabel = new QLabel(versionStr(), header);
        versionLabel->setObjectName("help-version-label");
        versionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        grid->addWidget(versionLabel, 1, 1, Qt::AlignTop | Qt::AlignHCenter);

        layout->addWidget(header);
    }

    // Tips
    {
        const int minHeight = 160;
        const QMargins margins(8, 0, 8, 8);

        auto *groupBox = new QGroupBox("Tips");
        groupBox->setMinimumHeight(minHeight);

        auto *gridLayout = new QGridLayout(groupBox);
        gridLayout->setContentsMargins(boxMargins);
        gridLayout->setSpacing(0);
        gridLayout->setColumnStretch(0, 1);
        gridLayout->setRowStretch(1, 1);

        auto *prevBtn = new QPushButton(backIcon(), "", groupBox);
        prevBtn->setFlat(true);

        auto *nextBtn = new QPushButton(nextIcon(), "", groupBox);
        nextBtn->setFlat(true);

        auto *currentNumber = new QLabel(groupBox);

        gridLayout->addWidget(prevBtn, 0, 1);
        gridLayout->addWidget(currentNumber, 0, 2, Qt::AlignCenter);
        gridLayout->addWidget(nextBtn, 0, 3);

        auto *currentTip = new QLabel(groupBox);
        currentTip->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        currentTip->setTextFormat(Qt::RichText);
        currentTip->setWordWrap(true);
        // currentTip->setTextBackgroundColor(Qt::transparent);
        currentTip->setTextInteractionFlags(Qt::NoTextInteraction);
        currentTip->setCursor({});
        currentTip->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        gridLayout->addWidget(currentTip, 1, 0, 1, 4);

        auto refereshTip = [this, currentNumber, currentTip]() {
            currentNumber->setText(QString::number(currentTipIdx() + 1) % "/" % QString::number(tips().size()));
            currentTip->setText(tips()[currentTipIdx()]);
        };

        refereshTip();
        QObject::connect(prevBtn, &QPushButton::clicked, prevBtn, [this]() { setCurrentTipIdx(currentTipIdx() - 1); });
        QObject::connect(nextBtn, &QPushButton::clicked, nextBtn, [this]() { setCurrentTipIdx(currentTipIdx() + 1); });
        QObject::connect(this, &HelpWindow::currentTipIdxChanged, groupBox, refereshTip);

        layout->addWidget(groupBox);
        layout->setStretch(layout->count() - 1, 1);
    }

    // Global Hotkeys
    {
        auto *groupBox = new QGroupBox("Global Hotkeys", this);
        if (!App::ghostRefInstance()->globalHotkeys()->isEnabled())
        {
            groupBox->setTitle(groupBox->title() + " (Disabled)");
        }

        auto *groupLayout = new QFormLayout(groupBox);
        groupLayout->setContentsMargins(boxMargins);
        groupLayout->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
        groupLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        auto *infoLabel = new QLabel("These can be used even when another application is active.", groupBox);
        infoLabel->setWordWrap(true);
        infoLabel->setAlignment(Qt::AlignCenter);
        infoLabel->setObjectName("help-info-label");
        groupLayout->addRow(infoLabel);

        addGlobalHotkeyRow(groupLayout, GlobalHotkeys::HideAllWindows);
        addGlobalHotkeyRow(groupLayout, GlobalHotkeys::ToggleGhostMode);

        layout->addWidget(groupBox);
    }

    // Hotkeys
    {
        BackWindowActions *windowActions = getBackWindowActions();
        const int marginTop = 5;

        auto *groupBox = new QGroupBox("Hotkeys", this);
        auto *groupLayout = new QFormLayout(groupBox);
        groupLayout->setContentsMargins(boxMargins.left(), marginTop, boxMargins.right(), boxMargins.bottom());
        groupLayout->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
        groupLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        groupLayout->addRow("Color Picker", hotkeyWidget(windowActions->colorPicker(), groupBox));
        groupLayout->addRow("Extract to New Window", hotkeyWidget(windowActions->extractTool(), groupBox));
        groupLayout->addRow("Hide Selected", hotkeyWidget(QKeySequence(Qt::Key_H), groupBox));
        groupLayout->addRow("Save Session", hotkeyWidget(windowActions->saveSession(), groupBox));
        groupLayout->addRow("Toggle Toolbar", hotkeyWidget(windowActions->toggleToolbar(), groupBox));

        layout->addWidget(groupBox);
    }

    layout->addStretch();
}

void HelpWindow::setCurrentTipIdx(int idx)
{
    const int numTips = static_cast<int>(tips().size());
    // Cycle tips
    idx %= numTips;
    if (idx < 0)
    {
        idx += numTips;
    }

    m_currentTipIdx = idx;
    emit currentTipIdxChanged(m_currentTipIdx);
}

const QList<QString> &HelpWindow::tips()
{
    if (!m_tips.isEmpty())
    {
        return m_tips;
    }

    m_tips.push_back(
        "Enter <b>Ghost Mode</b> by double clicking on a reference, "
        "pressing the <img src=:/ghost_mode.png width=24> button on the toolbar, or using the global hotkey <b>" %
        GlobalHotkeys::getKey(GlobalHotkeys::ToggleGhostMode).toString(QKeySequence::NativeText) % "</b>.");

    const QString overrideKeys = overrideKeysStr();
    m_tips.push_back("Ghost mode is deactivated whist holding <b>" % overrideKeys %
                     "</b> allowing references to be moved/resized etc. "
                     "Double clicking a reference in this state untoggles Ghost Mode."
                     "<br>The key(s) used can be changed in the <b>General</b> preferences tab.");

    const QString minimizeIcon = App::isDarkMode() ? "minimize_to_tray_dark.png" : "minimize_to_tray.png";
    m_tips.push_back("Minimize the toolbar to the system tray using the <img src=:/" % minimizeIcon %
                     " width=24> button. The toolbar can be restored by double clicking on the system tray icon.");

    const QString openShortcut = shortcutString(getBackWindowActions()->openAny().shortcut(), "No Shortcut");
    m_tips.push_back(QString("New reference images can be added by dragging an image onto the toolbar, copy/paste, or "
                             " clicking the toolbar's open button (<b>%0</b>).")
                         .arg(openShortcut));

    m_tips.push_back("Right clicking on a reference opens up the <b>settings panel</b> where various properties "
                     "(opacity, saturation, etc.) of the active reference can be altered.");

    m_tips.push_back("Dragging or paste an image onto an existing reference to open that image in a new tab. "
                     "Tabs can be detached using the <img src=:/detach_tab_btn width=20> button. Drag a detached "
                     "reference over another reference to merge them.");

    return m_tips;
}
