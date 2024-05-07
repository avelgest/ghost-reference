#include "preferences_window.h"

#include <QtGui/QScreen>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QSlider>
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

        QObject::connect(checkBox, &QCheckBox::stateChanged, [=]()
                         { prefs->setBool(key, checkBox->isChecked()); });

        m_layout->addWidget(checkBox);
        return checkBox;
    }

    QWidget *PrefWidgetMaker::createWidgetFloat()
    {
        const PrefFloatRange range = Preferences::getFloatRange(m_key);

        QScopedPointer<QHBoxLayout> hbox(new QHBoxLayout());

        auto *label = new QLabel(m_name + ":", parentWidget());

        auto *spinBox = new QDoubleSpinBox(parentWidget());
        spinBox->setSingleStep(range.size() / 100.);
        spinBox->setRange(range.min, range.max);
        spinBox->setToolTip(m_description);
        spinBox->setValue(m_prefs->getFloat(m_key));

        hbox->addWidget(label);
        hbox->addWidget(spinBox);

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

        case QMetaType::UnknownType:
        case QMetaType::Void:
            qCritical() << "Unable to find preference key" << key;
        default:
            return nullptr;
        }
    }

} // namespace

PreferencesWindow::PreferencesWindow(QWidget *parent)
    : PreferencesWindow(App::ghostRefInstance()->preferences(), parent)
{
}

PreferencesWindow::PreferencesWindow(Preferences *prefs, QWidget *parent)
    : QWidget(parent),
      m_prefs(prefs)
{
    setWindowFlag(Qt::Dialog);

    // Position the window above the main toolbar
    if (const MainToolbar *mainToolbar = App::ghostRefInstance()->mainToolbar(); isWindow() && mainToolbar)
    {
        move(mainToolbar->pos());
    }

    auto *layout = new PrefLayoutType(this);
    // layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    // layout->setLabelAlignment(Qt::AlignLeft);
    //  layout->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);

    PrefWidgetMaker widgetMaker(layout, prefs);

    widgetMaker.createWidget(Preferences::AllowInternet);
    widgetMaker.createWidget(Preferences::AnimateToolbarCollapse);
    widgetMaker.createWidget(Preferences::GhostModeOpacity);

    layout->addStretch();
}

void PreferencesWindow::setPrefs(Preferences *prefs)
{
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
