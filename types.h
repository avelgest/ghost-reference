#pragma once

class App;
class Preferences;
class ReferenceCollection;
class ReferenceImage;
class RefLoader;
class RefImageLoader;

// TODO Separate into widgets/types.h
// widgets
class BackWindow;
class PictureWidget;
class ReferenceWindow;
class ResizeFrame;
class ReferenceTitlebar;
class SettingsPanel;

// utils
namespace utils
{
    class NetworkDownload;
    class ZipFile;
} // namespace utils

#include <QtCore/QSharedPointer>
#include <QtCore/QWeakPointer>

typedef QSharedPointer<ReferenceImage> ReferenceImageSP;
typedef QWeakPointer<ReferenceImage> ReferenceImageWP;

enum WindowMode
{
    GhostMode,
    TransformMode,
};

enum class RefType
{
    Image,
};
