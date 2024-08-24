#pragma once

class App;
class Preferences;
class ReferenceCollection;
class ReferenceImage;
class RefLoader;
class RefImageLoader;
class UndoStack;

// TODO Separate into widgets/types.h
// widgets
class BackWindow;
class ColorPicker;
class MainToolbar;
class MainToolbarActions;
class PictureWidget;
class PreferencesWindow;
class ReferenceWindow;
class ResizeFrame;
class SettingsPanel;
class TabBar;
class Tool;

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
