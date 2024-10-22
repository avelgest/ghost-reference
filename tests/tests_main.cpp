
#include <gtest/gtest.h>

#include <QJsonDocument>

#include "../app.h"
#include "../global_hotkeys.h"
#include "../preferences.h"

namespace
{
    Preferences *testPreferences()
    {
        static Preferences *prefs = nullptr;

        if (!prefs)
        {
            prefs = new Preferences();
            prefs->setBool(Preferences::GlobalHotkeysEnabled, false);
        }
        return prefs;
    }
} // namespace

class AppEnv : public testing::Environment
{
    Q_DISABLE_COPY_MOVE(AppEnv)
    std::unique_ptr<App> m_app;

    int m_argc = 0;
    char **m_argv = nullptr;

public:
    AppEnv(int argc, char **argv)
        : m_argc(argc),
          m_argv(argv)
    {}
    ~AppEnv() override = default;

    void SetUp() override { m_app = std::make_unique<App>(m_argc, m_argv, App::ApplicationFlags, testPreferences()); }
    void TearDown() override { m_app.reset(); }
};

class PreferencesTests : public testing::Test
{
    Q_DISABLE_COPY_MOVE(PreferencesTests)
    Preferences m_prefs;

public:
    ~PreferencesTests() override = default;

protected:
    PreferencesTests()
    {
        // Set some preferences so m_prefs is different from the default
        m_prefs.setBool(Preferences::AnimateToolbarCollapse, true);
        m_prefs.globalHotkeys()[GlobalHotkeys::builtinName(GlobalHotkeys::HideAllWindows)] = Qt::Key_H;
    };

    Preferences &prefs() { return m_prefs; };
};

TEST_F(PreferencesTests, CheckAllEqual)
{
    const auto boolProp = Preferences::AllowInternet;
    Preferences first;
    Preferences second;

    EXPECT_TRUE(first.checkAllEqual(&second));
    EXPECT_TRUE(second.checkAllEqual(&first));

    first.setBool(boolProp, !first.getBool(boolProp));
    EXPECT_FALSE(first.checkAllEqual(&second));
    EXPECT_FALSE(second.checkAllEqual(&first));

    first.setBool(boolProp, second.getBool(boolProp));
    EXPECT_TRUE(first.checkAllEqual(&second));

    const auto globalHotkeyName = GlobalHotkeys::builtinName(GlobalHotkeys::HideAllWindows);
    first.globalHotkeys()[globalHotkeyName] = {Qt::Key_A};
    second.globalHotkeys()[globalHotkeyName] = {Qt::Key_B};
    EXPECT_FALSE(first.checkAllEqual(&second));

    first.globalHotkeys()[globalHotkeyName] = {Qt::Key_B};
    EXPECT_TRUE(first.checkAllEqual(&second));
}

TEST_F(PreferencesTests, Duplicate)
{
    const QScopedPointer<Preferences> dup(prefs().duplicate());
    EXPECT_TRUE(prefs().checkAllEqual(dup.get()));
}

TEST_F(PreferencesTests, CopyFromOther)
{
    Preferences newPrefs;
    newPrefs.copyFromOther(&prefs());
    EXPECT_TRUE(prefs().checkAllEqual(&newPrefs));
}

TEST_F(PreferencesTests, BoolProperty)
{
    const auto prop = Preferences::AllowInternet;
    EXPECT_TRUE(prefs().getType(prop) == QMetaType::Bool);

    prefs().setBool(prop, true);
    EXPECT_TRUE(prefs().getBool(prop));
    prefs().setBool(prop, false);
    EXPECT_FALSE(prefs().getBool(prop));
}

TEST_F(PreferencesTests, FloatProperty)
{
    const auto prop = Preferences::GhostModeOpacity;
    EXPECT_TRUE(prefs().getType(prop) == QMetaType::Double);

    const qreal testValue = 0.95;
    prefs().setFloat(prop, testValue);
    EXPECT_DOUBLE_EQ(prefs().getFloat(prop), testValue);

    auto range = Preferences::getFloatRange(prop);

    prefs().setFloat(prop, range.max + 1.0);
    EXPECT_DOUBLE_EQ(prefs().getFloat(prop), range.max);

    prefs().setFloat(prop, range.min - 1.0);
    EXPECT_DOUBLE_EQ(prefs().getFloat(prop), range.min);
}

TEST_F(PreferencesTests, JsonSaveLoad)
{
    QJsonDocument json = prefs().toJsonDocument();

    QScopedPointer<Preferences> newPrefs(Preferences::loadFromJson(json, nullptr));
    EXPECT_TRUE(prefs().checkAllEqual(newPrefs.get()));
}

int main(int argc, char *argv[])
{
    auto *appEnv = new AppEnv(argc, argv);

    testing::AddGlobalTestEnvironment(appEnv);
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}