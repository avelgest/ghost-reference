#include "image.h"

#include <ranges>

#include <QtCore/QDebug>
#include <QtGui/QImage>

void utils::reduceSaturation(QImage &image, qreal saturation)
{
    switch (image.format())
    {
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
        break;
    default:
        qWarning() << "Unsupported QImage format for reduceSaturation";
        return;
    }

    saturation = std::clamp(saturation, 0., 1.);

    const auto inverseSat = 1.F - static_cast<float>(saturation);

    for (int y = 0; y < image.height(); y++)
    {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); x++)
        {
            const QRgb rgb = line[x];
            const int red = qRed(rgb);
            const int green = qGreen(rgb);
            const int blue = qBlue(rgb);
            const int max = std::max(std::max(red, green), blue);

            line[x] = qRgba(red + static_cast<int>((max - red) * inverseSat),
                            green + static_cast<int>((max - green) * inverseSat),
                            blue + static_cast<int>((max - blue) * inverseSat), qAlpha(rgb));
        }
    }
}

bool utils::hasTransparentPixels(const QImage &image)
{
    if (!image.hasAlphaChannel()) return false;

    switch (image.format())
    {
    case QImage::Format_Alpha8:
        return true;
    case QImage::Format_Indexed8:
    {
        const QList<QRgb> colorTable = image.colorTable();
        return std::ranges::any_of(colorTable.cbegin(), colorTable.cend(),
                                   [](const QRgb &color) { return qAlpha(color) != UINT8_MAX; });
    }
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
    {
        for (int y = 0; y < image.height(); y++)
        {
            const auto *line = reinterpret_cast<const QRgb *>(image.scanLine(y));
            for (int x = 0; x < image.width(); x++)
            {
                if (qAlpha(line[x]) != UINT8_MAX) return true;
            }
        }
        return false;
    }
    case QImage::Format_RGBA64:
    case QImage::Format_RGBA64_Premultiplied:
        for (int y = 0; y < image.height(); y++)
        {
            const auto *line = reinterpret_cast<const QRgba64 *>(image.scanLine(y));
            for (int x = 0; x < image.width(); x++)
            {
                if (!line[x].isOpaque()) return true;
            }
        }
        return false;
    default:
    {
        const QImage imageCopy = image.convertedTo(QImage::Format_ARGB32_Premultiplied, Qt::NoOpaqueDetection);
        return hasTransparentPixels(imageCopy);
    }
    }
}
