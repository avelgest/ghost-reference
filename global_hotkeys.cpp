#include "global_hotkeys.h"

#include <QHotkey/QHotkey>

#include "app.h"
#include "preferences.h"

#include "widgets/back_window.h"
#include "widgets/main_toolbar_actions.h"

namespace
{
    App *getApp()
    {
        return App::ghostRefInstance();
    }

    MainToolbarActions *toolbarActions()
    {
        return getApp()->backWindow()->mainToolbarActions();
    }

} // namespace

const char *GlobalHotkeys::builtinName(Builtin enumValue)
{
    switch (enumValue)
    {
    case HideAllWindows:
        return "Hide all windows";
    }
    qCritical() << "No name found for built-in global hotkey" << enumValue;
    Q_ASSERT_X(false, "GlobalHotkeys::builtinName", "Not all Builtin global hotkeys have a name");
    return "Unknown";
}

QHotkey *GlobalHotkeys::addHotkey(const QKeySequence &keySequence)
{
    auto hotkey = std::make_unique<QHotkey>(keySequence, m_enabled);
    m_hotkeys.push_back(std::move(hotkey));
    return m_hotkeys.back().get();
}

void GlobalHotkeys::reloadAll()
{
    m_hotkeys.clear();

    const App *app = getApp();
    const Preferences *prefs = app->preferences();

    if (!prefs->getBool(Preferences::GlobalHotkeysEnabled))
    {
        return;
    }

    const Preferences::HotkeyMap &hotkeyMap = prefs->globalHotkeys();

    auto addBuiltinHotkey = [&](Builtin builtin) { return addHotkey(hotkeyMap[builtinName(builtin)]); };

    QHotkey *hotkey = nullptr;

    hotkey = addBuiltinHotkey(HideAllWindows);
    QObject::connect(hotkey, &QHotkey::activated, this, []() { toolbarActions()->toggleAllRefsHidden().trigger(); });
}

GlobalHotkeys::GlobalHotkeys(QObject *parent)
    : QObject(parent)
{
    const App *app = getApp();
    const Preferences *prefs = app->preferences();

    setEnabled(prefs->getBool(Preferences::GlobalHotkeysEnabled));

    reloadAll();

    QObject::connect(app, &App::preferencesReplaced, this, &GlobalHotkeys::reloadAll);
}

GlobalHotkeys::~GlobalHotkeys() = default;

void GlobalHotkeys::setEnabled(bool value)
{
    m_enabled = value;
    for (auto &hotkey : m_hotkeys)
    {
        hotkey->setRegistered(value);
    }
}