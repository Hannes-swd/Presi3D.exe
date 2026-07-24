#include "ZipWriter.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <array>
#include <cstdint>

namespace {

quint32 crc32Table[256];
bool     crc32TableReady = false;

void ensureCrc32Table() {
    if (crc32TableReady) return;
    for (quint32 i = 0; i < 256; ++i) {
        quint32 c = i;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc32Table[i] = c;
    }
    crc32TableReady = true;
}

quint32 crc32(const QByteArray& data) {
    ensureCrc32Table();
    quint32 c = 0xFFFFFFFFu;
    for (unsigned char byte : data)
        c = crc32Table[(c ^ byte) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void appendU16(QByteArray& buf, quint16 v) {
    buf.append(char(v & 0xFF));
    buf.append(char((v >> 8) & 0xFF));
}

void appendU32(QByteArray& buf, quint32 v) {
    buf.append(char(v & 0xFF));
    buf.append(char((v >> 8) & 0xFF));
    buf.append(char((v >> 16) & 0xFF));
    buf.append(char((v >> 24) & 0xFF));
}

// Fixed DOS date/time (1980-01-01 00:00:00) — timestamps are irrelevant here.
constexpr quint16 kDosTime = 0;
constexpr quint16 kDosDate = (1 << 5) | 1; // month=1, day=1, year=1980 (offset 0)

} // namespace

void ZipWriter::addFile(const QString& nameInZip, const QByteArray& data) {
    m_entries.append({ nameInZip, data });
}

bool ZipWriter::save(const QString& filePath) const {
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    QByteArray out;
    QVector<quint32> localHeaderOffsets;
    QVector<quint32> crcs;
    localHeaderOffsets.reserve(m_entries.size());
    crcs.reserve(m_entries.size());

    for (const Entry& e : m_entries) {
        const QByteArray nameUtf8 = e.name.toUtf8();
        const quint32    crc      = crc32(e.data);
        crcs.append(crc);
        localHeaderOffsets.append(quint32(out.size()));

        appendU32(out, 0x04034b50);          // local file header signature
        appendU16(out, 20);                  // version needed to extract
        appendU16(out, 0);                   // flags
        appendU16(out, 0);                   // compression method = stored
        appendU16(out, kDosTime);
        appendU16(out, kDosDate);
        appendU32(out, crc);
        appendU32(out, quint32(e.data.size())); // compressed size
        appendU32(out, quint32(e.data.size())); // uncompressed size
        appendU16(out, quint16(nameUtf8.size()));
        appendU16(out, 0);                   // extra field length
        out.append(nameUtf8);
        out.append(e.data);
    }

    const quint32 centralDirOffset = quint32(out.size());

    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry&  e       = m_entries[i];
        const QByteArray nameUtf8 = e.name.toUtf8();

        appendU32(out, 0x02014b50);          // central directory header signature
        appendU16(out, 20);                  // version made by
        appendU16(out, 20);                  // version needed to extract
        appendU16(out, 0);                   // flags
        appendU16(out, 0);                   // compression method = stored
        appendU16(out, kDosTime);
        appendU16(out, kDosDate);
        appendU32(out, crcs[i]);
        appendU32(out, quint32(e.data.size())); // compressed size
        appendU32(out, quint32(e.data.size())); // uncompressed size
        appendU16(out, quint16(nameUtf8.size()));
        appendU16(out, 0);                   // extra field length
        appendU16(out, 0);                   // comment length
        appendU16(out, 0);                   // disk number start
        appendU16(out, 0);                   // internal file attributes
        appendU32(out, 0);                   // external file attributes
        appendU32(out, localHeaderOffsets[i]);
        out.append(nameUtf8);
    }

    const quint32 centralDirSize = quint32(out.size()) - centralDirOffset;

    appendU32(out, 0x06054b50);              // end of central directory signature
    appendU16(out, 0);                       // disk number
    appendU16(out, 0);                       // disk with central directory
    appendU16(out, quint16(m_entries.size())); // entries on this disk
    appendU16(out, quint16(m_entries.size())); // total entries
    appendU32(out, centralDirSize);
    appendU32(out, centralDirOffset);
    appendU16(out, 0);                       // comment length

    const qint64 written = file.write(out);
    file.close();
    return written == out.size();
}
