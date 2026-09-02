#include "XlsxIO.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>

// Qt bundles zlib and exports it (as z_*, via Z_PREFIX in its own zconf.h), so
// the .xlsx container is packed and unpacked in this process.
//
// It used to be done by running `unzip -p` and `zip`, which is why saving a
// calibration result on Windows failed with "Could not write ...": neither
// program exists there. Reading failed the same way, silently -- a missing
// `unzip` and an empty spreadsheet are indistinguishable to a caller that only
// gets a grid back. Both had to be external only because the file avoided
// QXlsx and Qt's private headers; zlib is neither.
#include <zlib.h>

namespace {

// ---------------------------------------------------------------- zip ----
//
// Only what .xlsx needs: no encryption, no multi-disk, no ZIP64. Excel and
// openpyxl both write deflate; this app writes deflate too. Sizes and offsets
// come from the central directory rather than the local headers, because an
// entry written with a streaming data descriptor carries zeroes in its local
// header and the central directory is the copy that is always filled in.

quint16 le16(const QByteArray &b, int off) {
    if (off < 0 || off + 2 > b.size()) return 0;
    return quint16(quint8(b[off])) | quint16(quint8(b[off + 1])) << 8;
}

quint32 le32(const QByteArray &b, int off) {
    if (off < 0 || off + 4 > b.size()) return 0;
    return quint32(quint8(b[off])) | quint32(quint8(b[off + 1])) << 8 |
           quint32(quint8(b[off + 2])) << 16 | quint32(quint8(b[off + 3])) << 24;
}

void put16(QByteArray &b, quint16 v) {
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
}

void put32(QByteArray &b, quint32 v) {
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
    b.append(char((v >> 16) & 0xFF));
    b.append(char((v >> 24) & 0xFF));
}

// Raw DEFLATE (negative window bits): a zip member is bare deflate data, with
// no zlib header or trailer around it.
QByteArray inflateRaw(const QByteArray &in, quint32 outSize) {
    if (outSize == 0) return {};
    QByteArray out(int(outSize), Qt::Uninitialized);
    z_stream s = {};
    if (inflateInit2(&s, -MAX_WBITS) != Z_OK) return {};
    s.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
    s.avail_in = uInt(in.size());
    s.next_out = reinterpret_cast<Bytef *>(out.data());
    s.avail_out = uInt(out.size());
    const int r = inflate(&s, Z_FINISH);
    const uLong produced = s.total_out;
    inflateEnd(&s);
    if (r != Z_STREAM_END) return {};
    out.resize(int(produced));
    return out;
}

QByteArray deflateRaw(const QByteArray &in) {
    z_stream s = {};
    if (deflateInit2(&s, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK)
        return {};
    QByteArray out(int(deflateBound(&s, uLong(in.size())) + 16), Qt::Uninitialized);
    s.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
    s.avail_in = uInt(in.size());
    s.next_out = reinterpret_cast<Bytef *>(out.data());
    s.avail_out = uInt(out.size());
    const int r = deflate(&s, Z_FINISH);
    const uLong produced = s.total_out;
    deflateEnd(&s);
    if (r != Z_STREAM_END) return {};
    out.resize(int(produced));
    return out;
}

// One member of a .xlsx, uncompressed. Empty if the file or the member is not
// there -- the same "nothing to read" the callers already handle.
QByteArray zipMember(const QString &xlsx, const QString &member) {
    QFile f(xlsx);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray z = f.readAll();
    f.close();

    // End of central directory: fixed 22 bytes plus a trailing comment of up to
    // 64 KB, so it is found by scanning back for the signature.
    const int kEocdMin = 22;
    int eocd = -1;
    for (int i = z.size() - kEocdMin; i >= 0 && i >= z.size() - kEocdMin - 65536; --i)
        if (le32(z, i) == 0x06054b50) { eocd = i; break; }
    if (eocd < 0) return {};

    const quint16 count = le16(z, eocd + 10);
    int p = int(le32(z, eocd + 16));  // central directory offset

    const QByteArray want = member.toUtf8();
    for (quint16 i = 0; i < count; ++i) {
        if (le32(z, p) != 0x02014b50) return {};
        const quint16 method = le16(z, p + 10);
        const quint32 comp = le32(z, p + 20);
        const quint32 uncomp = le32(z, p + 24);
        const quint16 nameLen = le16(z, p + 28);
        const quint16 extraLen = le16(z, p + 30);
        const quint16 cmtLen = le16(z, p + 32);
        const quint32 localOff = le32(z, p + 42);
        const QByteArray name = z.mid(p + 46, nameLen);

        if (name == want) {
            if (le32(z, int(localOff)) != 0x04034b50) return {};
            // The local header has its own name/extra lengths, and the extra
            // field legitimately differs from the central copy -- so the data
            // offset must be computed from the local header, not that one.
            const quint16 lNameLen = le16(z, int(localOff) + 26);
            const quint16 lExtraLen = le16(z, int(localOff) + 28);
            const int data = int(localOff) + 30 + lNameLen + lExtraLen;
            if (data < 0 || data + int(comp) > z.size()) return {};
            const QByteArray raw = z.mid(data, int(comp));
            if (method == 0) return raw;             // stored
            if (method == 8) return inflateRaw(raw, uncomp);
            return {};                                // anything else: unsupported
        }
        p += 46 + nameLen + extraLen + cmtLen;
    }
    return {};
}

// Write a .xlsx: every member deflated, one local header each, then the central
// directory and the end record.
bool zipWrite(const QString &path, const QVector<QPair<QString, QByteArray>> &members) {
    QByteArray out;
    struct Entry {
        QByteArray name;
        quint32 crc, comp, uncomp, offset;
    };
    QVector<Entry> entries;
    entries.reserve(members.size());

    // A fixed 1980-01-01 stamp. Zero is not a legal DOS date (day 0), and the
    // timestamp is not information anyone reads off these files.
    const quint16 kDosTime = 0, kDosDate = 0x0021;

    for (const auto &m : members) {
        const QByteArray name = m.first.toUtf8();
        const QByteArray packed = deflateRaw(m.second);
        if (packed.isEmpty() && !m.second.isEmpty()) return false;

        Entry e;
        e.name = name;
        e.crc = quint32(crc32(0, reinterpret_cast<const Bytef *>(m.second.constData()),
                              uInt(m.second.size())));
        e.comp = quint32(packed.size());
        e.uncomp = quint32(m.second.size());
        e.offset = quint32(out.size());
        entries.append(e);

        put32(out, 0x04034b50);
        put16(out, 20);  // version needed
        put16(out, 0);   // flags
        put16(out, 8);   // deflate
        put16(out, kDosTime);
        put16(out, kDosDate);
        put32(out, e.crc);
        put32(out, e.comp);
        put32(out, e.uncomp);
        put16(out, quint16(name.size()));
        put16(out, 0);  // extra
        out.append(name);
        out.append(packed);
    }

    const quint32 cdOffset = quint32(out.size());
    for (const Entry &e : entries) {
        put32(out, 0x02014b50);
        put16(out, 20);  // version made by
        put16(out, 20);  // version needed
        put16(out, 0);   // flags
        put16(out, 8);   // deflate
        put16(out, kDosTime);
        put16(out, kDosDate);
        put32(out, e.crc);
        put32(out, e.comp);
        put32(out, e.uncomp);
        put16(out, quint16(e.name.size()));
        put16(out, 0);  // extra
        put16(out, 0);  // comment
        put16(out, 0);  // disk
        put16(out, 0);  // internal attrs
        put32(out, 0);  // external attrs
        put32(out, e.offset);
        out.append(e.name);
    }
    const quint32 cdSize = quint32(out.size()) - cdOffset;

    put32(out, 0x06054b50);
    put16(out, 0);
    put16(out, 0);
    put16(out, quint16(entries.size()));
    put16(out, quint16(entries.size()));
    put32(out, cdSize);
    put32(out, cdOffset);
    put16(out, 0);  // comment

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const qint64 n = f.write(out);
    // close() flushes; a full disk shows up here rather than as a truncated
    // file that only fails to open later, in Excel.
    return f.flush() && n == out.size() && (f.close(), true);
}

QByteArray unzipMember(const QString &xlsx, const QString &member) {
    return zipMember(xlsx, member);
}

// "B3" -> col=2, row=3 (both 1-based).
void parseRef(const QString &ref, int &col, int &row) {
    int i = 0;
    col = 0;
    while (i < ref.size() && ref[i].isLetter()) {
        col = col * 26 + (ref[i].toUpper().unicode() - 'A' + 1);
        ++i;
    }
    row = ref.mid(i).toInt();
}

void placeCell(XlsxIO::Grid &grid, int row, int col, const QString &text) {
    if (row < 1 || col < 1) return;
    while (grid.size() < row) grid.append(QVector<QString>());
    QVector<QString> &r = grid[row - 1];
    while (r.size() < col) r.append(QString());
    r[col - 1] = text;
}

QString colName(int col) {  // 1 -> "A", 27 -> "AA"
    QString s;
    while (col > 0) {
        int rem = (col - 1) % 26;
        s.prepend(QChar('A' + rem));
        col = (col - 1) / 26;
    }
    return s;
}

QString xmlEscape(const QString &s) {
    QString o = s;
    o.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;");
    return o;
}

}  // namespace

namespace {
// Resolve "xl/worksheets/sheetN.xml" for a named sheet via workbook.xml +
// its rels. Returns "" if not found.
QString resolveSheetFile(const QString &path, const QString &sheetName) {
    QString rid;
    {
        QXmlStreamReader xml(unzipMember(path, "xl/workbook.xml"));
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("sheet") &&
                xml.attributes().value("name") == sheetName) {
                rid = xml.attributes().value("r:id").toString();
                break;
            }
        }
    }
    if (rid.isEmpty()) return {};
    QXmlStreamReader xml(unzipMember(path, "xl/_rels/workbook.xml.rels"));
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("Relationship") &&
            xml.attributes().value("Id") == rid) {
            QString target = xml.attributes().value("Target").toString();
            // Target may be absolute ("/xl/worksheets/sheet1.xml", as openpyxl
            // writes) or relative to xl/ ("worksheets/sheet1.xml"). Zip members
            // are stored without a leading slash.
            if (target.startsWith("/")) target = target.mid(1);
            else if (!target.startsWith("xl/")) target = "xl/" + target;
            return target;
        }
    }
    return {};
}

// Resolve the FIRST worksheet (workbook.worksheets[0] in openpyxl) via the first
// <sheet> element's r:id -> its rels Target. Falls back to sheet1.xml.
QString resolveFirstSheetFile(const QString &path) {
    QString rid;
    {
        QXmlStreamReader xml(unzipMember(path, "xl/workbook.xml"));
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("sheet")) {
                rid = xml.attributes().value("r:id").toString();
                break;
            }
        }
    }
    if (rid.isEmpty()) return {};
    QXmlStreamReader xml(unzipMember(path, "xl/_rels/workbook.xml.rels"));
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("Relationship") &&
            xml.attributes().value("Id") == rid) {
            QString target = xml.attributes().value("Target").toString();
            // Target may be absolute ("/xl/worksheets/sheet1.xml", as openpyxl
            // writes) or relative to xl/ ("worksheets/sheet1.xml"). Zip members
            // are stored without a leading slash.
            if (target.startsWith("/")) target = target.mid(1);
            else if (!target.startsWith("xl/")) target = "xl/" + target;
            return target;
        }
    }
    return {};
}
}  // namespace

namespace XlsxIO {

Grid read(const QString &path, const QString &sheetName) {
    // Shared strings: concatenate <t> runs inside each <si>.
    QVector<QString> shared;
    {
        QXmlStreamReader xml(unzipMember(path, "xl/sharedStrings.xml"));
        QString cur;
        bool inSi = false;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("si")) {
                inSi = true;
                cur.clear();
            } else if (xml.isStartElement() && xml.name() == QLatin1String("t") && inSi) {
                cur += xml.readElementText();
            } else if (xml.isEndElement() && xml.name() == QLatin1String("si")) {
                shared.append(cur);
                inSi = false;
            }
        }
    }

    QString sheetFile = "xl/worksheets/sheet1.xml";
    if (!sheetName.isEmpty()) {
        const QString resolved = resolveSheetFile(path, sheetName);
        if (!resolved.isEmpty()) sheetFile = resolved;
    } else {
        // No name: use the first LOGICAL sheet (like openpyxl worksheets[0]),
        // which may not be sheet1.xml if sheets were reordered/deleted.
        const QString first = resolveFirstSheetFile(path);
        if (!first.isEmpty()) sheetFile = first;
    }

    Grid grid;
    QXmlStreamReader xml(unzipMember(path, sheetFile));
    QString ref, type;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("c")) {
            ref = xml.attributes().value("r").toString();
            type = xml.attributes().value("t").toString();
        } else if (xml.isStartElement() && xml.name() == QLatin1String("v")) {
            const QString v = xml.readElementText();
            const QString text = (type == "s") ? shared.value(v.toInt()) : v;
            int col, row;
            parseRef(ref, col, row);
            placeCell(grid, row, col, text);
        } else if (xml.isStartElement() && xml.name() == QLatin1String("is")) {
            // inline string: <is> contains child <t>, so include child text.
            const QString v = xml.readElementText(QXmlStreamReader::IncludeChildElements);
            int col, row;
            parseRef(ref, col, row);
            placeCell(grid, row, col, v);
        }
    }
    return grid;
}

bool write(const QString &path, const Grid &grid) {
    // The five parts of a minimal workbook, built in memory. There is no
    // scratch directory any more: they used to be written out as files only so
    // that an external `zip` could pick them up.
    QVector<QPair<QString, QByteArray>> parts;
    const auto put = [&parts](const QString &name, const QString &content) {
        parts.append({name, content.toUtf8()});
        return true;
    };

    put(QStringLiteral("[Content_Types].xml"),
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
        R"(<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>)"
        R"(<Default Extension="xml" ContentType="application/xml"/>)"
        R"(<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>)"
        R"(<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>)"
        R"(</Types>)");
    put(QStringLiteral("_rels/.rels"),
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
        R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>)"
        R"(</Relationships>)");
    put(QStringLiteral("xl/workbook.xml"),
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
        R"(<sheets><sheet name="Sheet" sheetId="1" r:id="rId1"/></sheets></workbook>)");
    put(QStringLiteral("xl/_rels/workbook.xml.rels"),
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
        R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>)"
        R"(</Relationships>)");

    // sheet1.xml with inline strings
    QString sheet =
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>)";
    for (int r = 0; r < grid.size(); ++r) {
        sheet += QString(R"(<row r="%1">)").arg(r + 1);
        const QVector<QString> &row = grid[r];
        for (int c = 0; c < row.size(); ++c) {
            if (row[c].isEmpty()) continue;
            sheet += QString(R"(<c r="%1%2" t="inlineStr"><is><t>%3</t></is></c>)")
                         .arg(colName(c + 1))
                         .arg(r + 1)
                         .arg(xmlEscape(row[c]));
        }
        sheet += "</row>";
    }
    sheet += "</sheetData></worksheet>";
    put(QStringLiteral("xl/worksheets/sheet1.xml"), sheet);

    return zipWrite(QFileInfo(path).absoluteFilePath(), parts);
}

}  // namespace XlsxIO
