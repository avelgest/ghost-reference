#include "undo_stack.h"

#include <map>
#include <ranges>

#include <QtCore/QJsonObject>

#include "app.h"
#include "preferences.h"
#include "reference_collection.h"
#include "reference_image.h"
#include "widgets/reference_window.h"

namespace
{
    class ReferenceEntry : public UndoStack::UndoEntry
    {
        QJsonObject m_json;
        ReferenceImageSP m_refImage;

    public:
        explicit ReferenceEntry(const ReferenceImageSP &refItem);
        bool undo() override;
        UndoStack::UndoEntryUP cloneAtPresent() const override;
    };

    class WindowEntry : public UndoStack::UndoEntry
    {
        RefWindowId m_identifier;
        QJsonObject m_json;
        QPointer<ReferenceWindow> m_refWindow;

    public:
        explicit WindowEntry(ReferenceWindow *refWindow);
        bool undo() override;
        UndoStack::UndoEntryUP cloneAtPresent() const override;
    };

    class ImageDataEntry : public UndoStack::UndoEntry
    {
        QImage m_imageData;
        ReferenceImageSP m_refImage;

    public:
        explicit ImageDataEntry(const ReferenceImageSP &refImage);
        bool undo() override;
        UndoStack::UndoEntryUP cloneAtPresent() const override;
        qint64 size() const override;
    };

    // Entry that restores the names of ReferenceImages and closes any new ReferenceWindows
    class GlobalStateEntry : public UndoStack::UndoEntry
    {
        QList<RefWindowId> m_windowIds;
        std::map<ReferenceImageSP, QString> m_referenceNames;

    public:
        GlobalStateEntry();
        bool undo() override;
        UndoStack::UndoEntryUP cloneAtPresent() const override;
    };

    ReferenceEntry::ReferenceEntry(const ReferenceImageSP &refItem)
        : m_refImage(refItem)
    {
        if (refItem) { m_json = refItem->toJson(); }
    }

    bool ReferenceEntry::undo()
    {
        if (m_refImage.isNull())
        {
            return false;
        }

        m_refImage->fromJson(m_json, nullptr);
        return true;
    }

    UndoStack::UndoEntryUP ReferenceEntry::cloneAtPresent() const
    {
        return std::make_unique<ReferenceEntry>(m_refImage);
    }

    WindowEntry::WindowEntry(ReferenceWindow *refWindow)
        : m_identifier(refWindow ? refWindow->identifier() : 0),
          m_refWindow(refWindow)
    {
        if (m_refWindow)
        {
            m_json = m_refWindow->toJson();
        }
    }

    bool WindowEntry::undo()
    {
        if (m_identifier == 0 || m_json.isEmpty())
        {
            return false;
        }
        ReferenceWindow *refWindow = App::ghostRefInstance()->getReferenceWindow(m_identifier);
        if (refWindow == nullptr)
        {
            refWindow = App::ghostRefInstance()->newReferenceWindow();
            refWindow->setIdentifier(m_identifier);
        }
        refWindow->fromJson(m_json);
        refWindow->setVisible(!refWindow->ghostRefHidden());

        return true;
    }

    UndoStack::UndoEntryUP WindowEntry::cloneAtPresent() const
    {
        return std::make_unique<WindowEntry>(m_refWindow.get());
    }

    ImageDataEntry::ImageDataEntry(const ReferenceImageSP &refImage)
        : m_refImage(refImage)
    {
        if (refImage) 
        {
            m_imageData = refImage->baseImage(); 
        }
    }

    bool ImageDataEntry::undo()
    {
        if (!m_refImage) { return false; }

        m_refImage->setBaseImage(m_imageData);
        return true;
    }

    UndoStack::UndoEntryUP ImageDataEntry::cloneAtPresent() const
    {
        return std::make_unique<ImageDataEntry>(m_refImage);
    }

    qint64 ImageDataEntry::size() const
    {
        return m_imageData.sizeInBytes();
    }

    GlobalStateEntry::GlobalStateEntry()
    {
        const App *app = App::ghostRefInstance();
        for (const auto &refWin : app->referenceWindows())
        {
            if (refWin)
            {
                m_windowIds.push_back(refWin->identifier());
            }
        }
        for (const auto &refItem : app->referenceItems()->references())
        {
            m_referenceNames[refItem] = refItem->name();
        }
    }

    bool GlobalStateEntry::undo()
    {
        App *app = App::ghostRefInstance();
        // Remove all ReferenceWindows without identifiers in m_windowIds
        for (const auto &refWin : app->referenceWindows())
        {
            if (refWin && !m_windowIds.contains(refWin->identifier()))
            {
                refWin->close();
            }
        }
        // FIXME When redoing a ReferenceWindow duplication the window is not recreated

        // Rename all reference items to their names when this entry was created
        for (const auto &[refItem, name] : m_referenceNames)
        {
            app->referenceItems()->renameReference(*refItem, name, true);
        }

        return true;
    }

    UndoStack::UndoEntryUP GlobalStateEntry::cloneAtPresent() const
    {
        return std::make_unique<GlobalStateEntry>();
    }

} // namespace

void UndoStack::addUndoStep(UndoStep &&undoStep)
{
    const int maxSteps = appPrefs()->getInt(Preferences::UndoMaxSteps);

    if (maxSteps <= 0)
    {
        m_undoStack.clear();
        m_redoStack.clear();
        return;
    }

    m_undoStack.push_back(std::move(undoStep));
    m_redoStack.clear();

    // Check the number of steps
    if (m_undoStack.size() > maxSteps)
    {
        const int eraseCount = static_cast<int>(m_undoStack.size() - maxSteps);
        m_undoStack.erase(m_undoStack.begin(), m_undoStack.begin() + eraseCount);
    }

    App::ghostRefInstance()->setUnsavedChanges();
}

UndoStack *UndoStack::get()
{
    return App::ghostRefInstance()->undoStack();
}

UndoStack::UndoStack(QObject *parent)
    : QObject(parent)
{}

void UndoStack::pushGlobalUndo()
{
    App *app = App::ghostRefInstance();
    UndoStep undoStep;

    undoStep.addEntry(std::make_unique<GlobalStateEntry>());
    for (const ReferenceImageSP &refItem : app->referenceItems()->references())
    {
        undoStep.addEntry(std::make_unique<ReferenceEntry>(refItem));
    }

    for (ReferenceWindow *refWindow : app->referenceWindows())
    {
        undoStep.addEntry(std::make_unique<WindowEntry>(refWindow));
    }

    addUndoStep(std::move(undoStep));
}

void UndoStack::pushImageData(const ReferenceImageSP &refImage)
{
    if (!refImage) { return; }

    UndoStep undoStep;
    undoStep.addEntry(std::make_unique<ImageDataEntry>(refImage));
    addUndoStep(std::move(undoStep));
}

void UndoStack::pushRefItem(const ReferenceImageSP &refItem, bool imageData)
{
    if (!refItem) { return; }
    
    UndoStep undoStep;
    undoStep.addEntry(std::make_unique<ReferenceEntry>(refItem));
    if (imageData)
    {
        undoStep.addEntry(std::make_unique<ImageDataEntry>(refItem));
    }

    addUndoStep(std::move(undoStep));
}

void UndoStack::pushRefWindow(ReferenceWindow *refWindow, bool refItems)
{
    if (!refWindow)
    {
        return;
    }

    UndoStep undoStep;
    if (refItems)
    {
        for (const auto &item : refWindow->referenceImages())
        {
            undoStep.addEntry(std::make_unique<ReferenceEntry>(item));
        }
    }
    undoStep.addEntry(std::make_unique<WindowEntry>(refWindow));
    addUndoStep(std::move(undoStep));
}

void UndoStack::pushWindowAndRefItem(ReferenceWindow *refWindow, const ReferenceImageSP &refItem, bool imageData)
{
    if (!refWindow && !refItem) { return; }

    UndoStep undoStep;
    if (refItem)
    {
        undoStep.addEntry(std::make_unique<ReferenceEntry>(refItem));
        if (imageData)
        {
            undoStep.addEntry(std::make_unique<ImageDataEntry>(refItem));
        }
    }

    undoStep.addEntry(std::make_unique<WindowEntry>(refWindow));
    addUndoStep(std::move(undoStep));
}

bool UndoStack::undo()
{
    if (m_undoStack.empty())
    {
        return false;
    }
    UndoStep &undoStep = m_undoStack.back();
    UndoStep redoStep = undoStep.cloneAtPresent();
    undoStep.perform();
    m_undoStack.pop_back();
    m_redoStack.push_back(std::move(redoStep));

    emit undone();
    emit undoneOrRedone();
    return true;
}

bool UndoStack::redo()
{
    if (m_redoStack.empty())
    {
        return false;
    }
    UndoStep &redoStep = m_redoStack.back();
    UndoStep undoStep = redoStep.cloneAtPresent();

    redoStep.perform();
    m_redoStack.pop_back();
    m_undoStack.push_back(std::move(undoStep));

    emit redone();
    emit undoneOrRedone();
    return true;
}

void UndoStack::UndoStep::addEntry(UndoEntryUP &&entry)
{
    m_entries.push_back(std::move(entry));
}

UndoStack::UndoStep UndoStack::UndoStep::cloneAtPresent() const
{
    UndoStep cloned;
    for (const auto &entry : m_entries)
    {
        cloned.addEntry(entry->cloneAtPresent());
    }
    return cloned;
}

void UndoStack::UndoStep::perform()
{
    for (auto &entry : m_entries)
    {
        entry->undo();
    }
}

qint64 UndoStack::UndoStep::size() const
{
    return std::transform_reduce(m_entries.cbegin(), m_entries.cend(), 0LL, std::plus{},
                                 [](const UndoEntryUP &entry) { return entry->size(); });
}
