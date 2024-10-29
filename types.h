#pragma once

#ifndef GHOST_REF_VERSION_MAJOR
#define GHOST_REF_VERSION_MAJOR 0
#define GHOST_REF_VERSION_MINOR 0
#define GHOST_REF_VERSION_PATCH 0
#endif // !GHOST_REF_VERSION_MAJOR

class App;
class GlobalHotkeys;
class Preferences;
class ReferenceCollection;
class ReferenceImage;
class RefLoader;
class RefImageLoader;
class SystemTrayIcon;
class UndoStack;

// tools
class Tool;
class ColorPicker;

// TODO Separate into widgets/types.h
// widgets
class BackWindow;
class MainToolbar;
class BackWindowActions;
class PictureWidget;
class PreferencesWindow;
class ReferenceWindow;
class ResizeFrame;
class SettingsPanel;
class TabBar;

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

typedef qint64 RefWindowId;

enum WindowMode
{
    GhostMode,
    TransformMode,
    ToolMode,
};

enum class RefType
{
    Image,
};
