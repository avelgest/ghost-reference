#include "zip_file.h"

#include <algorithm>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QList>
#include <QtCore/Qt>

#include <mz.h>
#include <mz_os.h>
#include <mz_strm.h>
#include <mz_strm_mem.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

using ZipFile = utils::ZipFile;
using FileEntry = ZipFile::FileEntry;

namespace
{
    const uint16_t compressMethod = MZ_COMPRESS_METHOD_STORE;
    const uint16_t compressLevel = 6;

    template <typename T, T (*create)(void), void (*destruct)(T *)>
    class CreateDelete
    {
    private:
        T m_value;

    public:
        typedef CreateDelete<T, create, destruct> Type;

        CreateDelete<T, create, destruct>() : m_value(create()) {}
        ~CreateDelete<T, create, destruct>() { destroy(); }

        CreateDelete<T, create, destruct>(const Type &) = delete;
        Type &operator=(const Type &) = delete;

        CreateDelete<T, create, destruct>(Type &&other) noexcept : m_value(other.take()) {}

        Type &operator=(Type &&other) noexcept
        {
            destroy();
            m_value = other.take();
            return *this;
        }

        void destroy() noexcept { destruct(&m_value); }
        T get() const noexcept { return m_value; }
        T take() noexcept
        {
            T tmp = m_value;
            m_value = NULL;
            return tmp;
        }
    };

    using MemStream = CreateDelete<void *, mz_stream_mem_create, mz_stream_mem_delete>;
    using ZipHandle = CreateDelete<void *, mz_zip_create, mz_zip_delete>;
    using ZipReader = CreateDelete<void *, mz_zip_reader_create, mz_zip_reader_delete>;
    using ZipWriter = CreateDelete<void *, mz_zip_writer_create, mz_zip_writer_delete>;

    QByteArray readEntryData(const ZipReader &zipReader) noexcept
    {
        int32_t err = MZ_OK;
        if (err = mz_zip_reader_entry_open(zipReader.get()); err != MZ_OK)
        {
            qCritical() << "Unable to open zip reader entry (" << err << ")";
            return {};
        }

        QByteArray entryData;
        const int32_t bufSize = 4096;
        std::array<char, bufSize> buf = {};

        while ((err = mz_zip_reader_entry_read(zipReader.get(), buf.data(), bufSize)) > 0)
        {
            const int32_t bytesRead = err;
            entryData.append(buf.data(), bytesRead);
        }
        if (err < 0)
        {
            qCritical() << "Error reading zip reader entry (" << err << ")";
            return {};
        }

        return entryData;
    }

    void writeEntry(const ZipWriter &zipWriter, ZipFile::FileEntry &fileEntry) noexcept
    {
        if (fileEntry.data.size() > INT32_MAX)
        {
            qCritical() << "Unable to add entry" << fileEntry.filename << "to zip. Entry too large";
            return;
        }

        const QByteArray filename = fileEntry.filename.toUtf8();
        const std::time_t timeNow = std::time(nullptr);

        mz_zip_file fileInfo = {0};
        fileInfo.compression_method = compressMethod;
        fileInfo.filename = filename.constData();
        fileInfo.flag = MZ_ZIP_FLAG_UTF8;
        fileInfo.creation_date = timeNow;
        fileInfo.modified_date = timeNow;
        fileInfo.version_madeby = MZ_HOST_SYSTEM(MZ_VERSION_MADEBY);

        auto *buf = static_cast<void *>(fileEntry.data.data());
        const auto bufSize = static_cast<int32_t>(fileEntry.data.size());

        if (const int32_t err = mz_zip_writer_add_buffer(zipWriter.get(), buf, bufSize, &fileInfo); err != MZ_OK)
        {
            qCritical() << "Error adding zip entry for" << fileEntry.filename << "Error code:" << err;
        }
    }

    // Returns a QList of pointers to FileEntries such that the total size of all entries
    // is less than around 2GB
    QList<FileEntry *> prunedEntries(QList<FileEntry> &entries)
    {
        // TODO Move to checking to ZipFile::addFile
        const qsizetype maxSize = INT32_MAX - (1U << 20U); // Leave space for zip metadata
        qsizetype totalSize = 0;
        QList<FileEntry *> pruned;

        for (auto &entry : entries)
        {
            pruned.push_back(&entry);
            totalSize += entry.data.size();
        }

        if (totalSize > maxSize)
        {
            qWarning() << "Session File is too large. Some images will be discarded.";

            // Sort pruned from smallest entry to largest
            std::sort(pruned.begin(), pruned.end(), [](FileEntry *a, FileEntry *b)
                      { return a->data.size() < b->data.size(); });
            while (totalSize > maxSize)
            {
                FileEntry *largest = pruned.takeLast();
                if (!largest)
                {
                    break;
                }
                totalSize -= largest->data.size();
            }
        }

        return pruned;
    }

    QString cleanPath(const QString &path)
    {
        return QDir::cleanPath(path);
    }

} // namespace

ZipFile ZipFile::fromBuffer(QByteArray &buffer)
{
    if (buffer.count() > INT32_MAX)
    {
        qCritical() << "Memory buffer too large to open as a ZipFile";
        return {};
    }

    const ZipReader zipReader;
    int32_t err = MZ_OK;

    auto *data = reinterpret_cast<uint8_t *>(buffer.data());
    auto dataLen = static_cast<int32_t>(buffer.count());

    if (err = mz_zip_reader_open_buffer(zipReader.get(), data, dataLen, 0); err != MZ_OK)
    {
        qCritical() << "minizip: Error opening buffer for reading " << err;
        return {};
    }

    if (err = mz_zip_reader_goto_first_entry(zipReader.get()); err != MZ_OK)
    {
        if (err != MZ_END_OF_LIST)
        {
            qCritical() << "Error finding first entry in ZipFile" << err;
        }
        return {}; // N.B. zipReader is closed in mz_zip_reader_delete
    }

    ZipFile zipFile;
    do
    {
        mz_zip_file *fileInfo = nullptr;
        if (err = mz_zip_reader_entry_get_info(zipReader.get(), &fileInfo); err == MZ_OK)
        {
            QByteArray entryData = std::move(readEntryData(zipReader));
            zipFile.addFile(fileInfo->filename, std::move(entryData));
        }
    } while (mz_zip_reader_goto_next_entry(zipReader.get()) == MZ_OK);

    mz_zip_reader_close(zipReader.get());
    return zipFile;
}

QByteArray ZipFile::toBuffer()
{
    int32_t err = MZ_OK;

    const MemStream memStream;
    const int32_t growSize = 128 * 1024;
    mz_stream_mem_set_grow_size(memStream.get(), growSize);

    {
        const ZipWriter zipWriter;
        mz_zip_writer_set_compress_method(zipWriter.get(), compressMethod);
        mz_zip_writer_set_compress_level(zipWriter.get(), compressLevel);

        if (err = mz_stream_mem_open(memStream.get(), nullptr, MZ_OPEN_MODE_CREATE); err != MZ_OK)
        {
            qCritical() << "Unable to open minizip memory stream for writing. Error code:" << err;
            return {};
        }
        mz_stream_mem_seek(memStream.get(), 0, MZ_SEEK_SET);

        if (err = mz_zip_writer_open(zipWriter.get(), memStream.get(), 0); err != MZ_OK)
        {
            qCritical() << "Unable to open minizip ZipWriter. Error code:" << err;
            return {};
        }

        for (auto *entry : prunedEntries(m_fileEntries))
        {
            writeEntry(zipWriter, *entry);
        }

        mz_zip_writer_close(zipWriter.get());
    }

    // TODO: Total file size > INT32_MAX not supported

    const void *buf = nullptr;
    if (err = mz_stream_mem_get_buffer(memStream.get(), &buf); err != MZ_OK)
    {
        qCritical() << "Unable to get minizip memory stream buffer. Error code:" << err;
        return {};
    }

    int32_t bufSize = 0;
    mz_stream_mem_get_buffer_length(memStream.get(), &bufSize);

    QByteArray out(static_cast<const char *>(buf), bufSize);
    return out;
}

void ZipFile::addFile(const QString &filename, const QByteArray &data)
{
    addFile(filename, std::move(QByteArray(data)));
}

void ZipFile::addFile(const QString &filename, QByteArray &&data)
{
    const QString cleanName = cleanPath(filename);

    for (auto &entry : m_fileEntries)
    {
        if (entry.filename == cleanName)
        {
            entry = std::move(FileEntry(cleanName, std::move(data)));
            return;
        }
    }
    m_fileEntries.push_back({cleanName, std::move(data)});
}

const QByteArray &utils::ZipFile::getFile(const QString &filename) const
{
    static const QByteArray nullArray; // Returned if the file can't be found

    const QString cleanName = cleanPath(filename);

    for (const auto &entry : m_fileEntries)
    {
        if (entry.filename == cleanName)
        {
            return entry.data;
        }
    }
    return nullArray;
}

bool ZipFile::hasFile(const QString &filename) const
{
    const QString cleanName = cleanPath(filename);

    return std::ranges::any_of(m_fileEntries, [cleanName](const FileEntry &x)
                               { return x.filename == cleanName; });
}
