#pragma once

#include <QtCore/QList>

class QByteArray;
class QString;

namespace utils
{

    class ZipFile
    {
    public:
        struct FileEntry
        {
            QString filename;
            QByteArray data;
        };

    private:
        QList<FileEntry> m_fileEntries;

    public:
        ZipFile() = default;
        ~ZipFile() = default;

        ZipFile(const ZipFile &) = delete;
        ZipFile &operator=(const ZipFile &) = delete;

        ZipFile(ZipFile &&other) = default;
        ZipFile &operator=(ZipFile &&other) = default;

        static ZipFile fromBuffer(QByteArray &buffer);
        QByteArray toBuffer();

        void addFile(const QString &filename, const QByteArray &data);
        void addFile(const QString &filename, QByteArray &&data);

        const QByteArray &getFile(const QString &filename) const;
        bool hasFile(const QString &filename) const;
        bool isEmpty() const;
    };

    inline bool ZipFile::isEmpty() const { return m_fileEntries.isEmpty(); }

} // namespace utils
