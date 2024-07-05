#include "color_picker.h"

#include <QtGui/QClipboard>
#include <QtGui/QMouseEvent>
#include <QtGui/QPalette>

#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>

#include "app.h"
#include "reference_image.h"
#include "widgets/back_window.h"
#include "widgets/picture_widget.h"
#include "widgets/reference_window.h"

namespace
{
    const QChar degChar(0xb0); // Unicode degree character

    BackWindow *backWindow()
    {
        return App::ghostRefInstance()->backWindow();
    }

    QString hsvString(const QColor &color)
    {
        const int percent = 100;
        const int maxHue = 359;

        const QColor hsv = color.toHsv();
        const int hue = color.hsvHue();
        return QString("%1%4  %2%  %3%")
            .arg((hue < 0) ? (maxHue - hue) % (maxHue + 1) : hue)
            .arg(qRound(hsv.hslSaturationF() * percent))
            .arg(qRound(hsv.valueF() * percent))
            .arg(degChar);
    }

    QString hslString(const QColor &color)
    {
        const int percent = 100;
        const int maxHue = 359;

        const QColor hsl = color.toHsl();
        const int hue = hsl.hslHue();

        return QString("%1%4  %2%  %3%")
            .arg((hue < 0) ? (maxHue - hue) % (maxHue + 1) : hue)
            .arg(qRound(hsl.hslSaturationF() * percent))
            .arg(qRound(hsl.lightnessF() * percent))
            .arg(degChar);
    }

    QString rgbPercentString(const QColor &color)
    {
        const int percent = 100;
        return QString("%1%  %2%  %3%")
            .arg(qRound(color.redF() * percent))
            .arg(qRound(color.greenF() * percent))
            .arg(qRound(color.blueF() * percent));
    }

    class ColorPickerWindow : public QFrame
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(ColorPickerWindow)

    private:
        ColorPicker *m_colorPicker;
        QFrame *m_colorPatch;
        QLineEdit *m_rgbText;
        QLineEdit *m_rgbPercentText;
        QLineEdit *m_hsvText;
        QLineEdit *m_hslText;

        QColor m_color;

    public:
        explicit ColorPickerWindow(ColorPicker *colorPicker);
        ~ColorPickerWindow() override = default;

    protected:
        void setColor(const QColor &color);

    private:
        QPushButton *createCopyButton(QLineEdit *target);
        QLineEdit *createValuesTextBox();
    };

    ColorPickerWindow::ColorPickerWindow(ColorPicker *colorPicker)
        : QFrame(backWindow(), Qt::Tool | Qt::WindowStaysOnTopHint),
          m_colorPicker(colorPicker),
          m_colorPatch(new QFrame(this)),
          m_rgbText(createValuesTextBox()),
          m_rgbPercentText(createValuesTextBox()),
          m_hsvText(createValuesTextBox()),
          m_hslText(createValuesTextBox())
    {
        const QSize minSize(180, 100);
        const QSize maxSize(512, 256);
        const QSize colorPatchSize(48, 32);

        setMinimumSize(minSize);
        setMaximumSize(maxSize);
        setWindowTitle("Color Picker");

        auto *layout = new QGridLayout(this);

        m_colorPatch->setAutoFillBackground(true);
        m_colorPatch->setMinimumSize(colorPatchSize);
        m_colorPatch->setObjectName("color-picker-patch");
        layout->addWidget(m_colorPatch, 0, 0, -1, 1);

        layout->addWidget(new QLabel("RGB:", this), 0, 1);
        layout->addWidget(new QLabel("RGB (%):", this), 1, 1);
        layout->addWidget(new QLabel("HSV:", this), 2, 1);
        layout->addWidget(new QLabel("HSL:", this), 3, 1);

        layout->addWidget(m_rgbText, 0, 2);
        layout->addWidget(m_rgbPercentText, 1, 2);
        layout->addWidget(m_hsvText, 2, 2);
        layout->addWidget(m_hslText, 3, 2);

        layout->addWidget(createCopyButton(m_rgbText), 0, 3);

        setColor(Qt::black);

        QObject::connect(m_colorPicker, &ColorPicker::colorPicked, this, &ColorPickerWindow::setColor);
    }

    void ColorPickerWindow::setColor(const QColor &color)
    {
        m_color = color;
        const QString rgbStr = color.name();
        m_rgbText->setText(rgbStr);
        m_rgbPercentText->setText(rgbPercentString(color));
        m_hsvText->setText(hsvString(color));
        m_hslText->setText(hslString(color));
        m_colorPatch->setStyleSheet("background-color: " + rgbStr);
        m_colorPatch->update();
    }

    QPushButton *ColorPickerWindow::createCopyButton(QLineEdit *target)
    {
        auto *copyBtn = new QPushButton("Copy", this);

        QObject::connect(copyBtn, &QPushButton::clicked, this,
                         [target]() { QGuiApplication::clipboard()->setText(target->text()); });
        return copyBtn;
    }

    QLineEdit *ColorPickerWindow::createValuesTextBox()
    {
        auto *widget = new QLineEdit(this);
        widget->setReadOnly(true);
        return widget;
    }

    QColor pickColor(PictureWidget *widget, QPointF localPos)
    {
        if (const ReferenceImageSP &refImage = widget->image(); refImage)
        {
            const QImage &image = refImage->displayImage();
            const QPoint imgPos = widget->localToImage(localPos).toPoint();
            if (image.rect().contains(imgPos))
            {
                return image.pixel(imgPos);
            }
        }
        return {};
    }

    bool underMouseEvent(const QWidget *widget, const QMouseEvent *event)
    {
        const QRect contentRect = widget->contentsRect();
        const QRect globalRect(widget->mapToGlobal(contentRect.topLeft()), contentRect.size());
        return globalRect.contains(event->globalPos());
    }

} // namespace

void ColorPicker::onActivate()
{
    Tool::onActivate();
    m_toolWindow = std::make_unique<ColorPickerWindow>(this);
    m_toolWindow->show();
}

void ColorPicker::onDeactivate()
{
    Tool::onDeactivate();
}

ColorPicker::ColorPicker()
{
    setCursor(Qt::CrossCursor);
}

QIcon ColorPicker::icon()
{
    return App::style()->standardIcon(QStyle::SP_MessageBoxCritical);
}

void ColorPicker::mouseMoveEvent(QWidget *widget, QMouseEvent *event)
{
    if (event->buttons().testFlag(Qt::LeftButton) && underMouseEvent(widget, event))
    {
        auto *picWidget = qobject_cast<PictureWidget *>(widget);
        if (picWidget != nullptr)
        {
            const QColor color = pickColor(picWidget, event->position());
            emit colorPicked(color);
            event->accept();
        }
    }
}

void ColorPicker::mouseReleaseEvent(QWidget *widget, QMouseEvent *event)
{
    auto *picWidget = qobject_cast<PictureWidget *>(widget);
    if (picWidget == nullptr)
    {
        return;
    }

    event->accept();

    if (event->button() == Qt::LeftButton)
    {
        const QColor color = pickColor(picWidget, event->position());
        emit colorPicked(color);

        m_toolWindow->show();
    }
    else if (event->button() == Qt::RightButton)
    {
        deactivate();
    }
}

void ColorPicker::keyReleaseEvent([[maybe_unused]] QWidget *widget, QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        deactivate();
    }
}

#include "color_picker.moc"