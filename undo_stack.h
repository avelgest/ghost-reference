#pragma once

#include <memory>
#include <vector>

#include <QtCore/QObject>

#include "types.h"

class UndoStack : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(UndoStack)

public:
    class UndoEntry;
    using UndoEntryUP = std::unique_ptr<UndoEntry>;

    class UndoEntry
    {
        Q_DISABLE_COPY_MOVE(UndoEntry)
    public:
        UndoEntry() = default;
        virtual ~UndoEntry() = default;
        virtual bool undo() = 0;
        // Create a new UndoEntry with the same settings, but for the application's current state.
        virtual UndoEntryUP cloneAtPresent() const = 0;

        virtual qint64 size() const { return 0; }
    };

protected:
    class UndoStep
    {
        std::vector<UndoEntryUP> m_entries;

    public:
        void addEntry(UndoEntryUP &&entry);
        UndoStep cloneAtPresent() const;
        void perform();
        qint64 size() const;
    };

    void addUndoStep(UndoStep &&undoStep);

private:
    std::vector<UndoStep> m_undoStack;
    std::vector<UndoStep> m_redoStack;

public:
    // Same as App::ghostRefInstance()->undoStack()
    static UndoStack *get();

    explicit UndoStack(QObject *parent = nullptr);
    ~UndoStack() override = default;

    // Adds an undo step for all ReferenceWindows and ReferenceImages.
    void pushGlobalUndo();

    // Adds an undo step for the baseImage property of refImage
    void pushImageData(const ReferenceImageSP &refImage);

    // Adds an undo step for a ReferenceImage. If imageData is true then the image data
    // (i.e. baseImage property) is included.
    void pushRefItem(const ReferenceImageSP &refItem, bool imageData = false);

    // Adds an undo step for a ReferenceWindow. If refItems is true then all reference images used
    // by the window are included in the undo step.
    void pushRefWindow(ReferenceWindow *refWindow, bool refItems = false);

    // Adds a combined undo step for a ReferenceWindow and ReferenceImage. If imageData is true
    // then the image data (i.e. baseImage property) of refItem is included.
    void pushWindowAndRefItem(ReferenceWindow *refWindow, const ReferenceImageSP &refItem, bool imageData = false);

    bool undo();
    bool redo();

    // Delete all undo/redo steps
    void clear();

signals:
    void undone();
    void redone();
    void undoneOrRedone();
};