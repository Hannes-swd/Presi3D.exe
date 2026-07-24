#pragma once
#include <QString>
#include <QByteArray>
#include <QVector>

// Minimal, dependency-free ZIP writer (no zlib required): every entry is
// stored uncompressed (method 0). That is perfectly valid ZIP and is all
// OOXML formats (.docx/.pptx/.xlsx) need, since consumers only care that the
// archive is well-formed, not that it is compressed.
class ZipWriter {
public:
    // nameInZip uses forward slashes, e.g. "word/document.xml".
    void addFile(const QString& nameInZip, const QByteArray& data);

    // Writes the archive to filePath. Returns false on I/O failure.
    bool save(const QString& filePath) const;

private:
    struct Entry {
        QString    name;
        QByteArray data;
    };
    QVector<Entry> m_entries;
};
