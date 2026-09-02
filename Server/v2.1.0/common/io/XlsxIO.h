#pragma once

#include <QString>
#include <QVector>

// Minimal .xlsx read/write using the system unzip/zip + Qt's public XML —
// no QXlsx, no Qt private headers. A sheet is a row-major grid of cell strings.
namespace XlsxIO {

using Grid = QVector<QVector<QString>>;

// Read a worksheet of an .xlsx file as a row-major grid (1-based Excel cells
// map to 0-based grid indices). If `sheetName` is given it is resolved via the
// workbook; otherwise the first sheet is read. Empty grid on failure.
Grid read(const QString &path, const QString &sheetName = QString());

// Write a grid as a single-sheet .xlsx (inline strings). Returns false on error.
bool write(const QString &path, const Grid &grid);

}  // namespace XlsxIO
