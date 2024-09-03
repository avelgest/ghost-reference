#pragma once

#include <QtCore/qtypes.h>

class QImage;

namespace utils
{
    // Reduces the saturation of image to saturation * it's current value.
    void reduceSaturation(QImage &image, qreal saturation);

    // Returns true if image has any pixels that are not opaque
    bool hasTransparentPixels(const QImage &image);

} // namespace utils