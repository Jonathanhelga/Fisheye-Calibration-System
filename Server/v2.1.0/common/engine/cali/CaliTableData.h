#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include "CaliDataSource.h"

// In-memory CaliDataSource: the 11 round tables and the form's scalar inputs as
// plain data, plus a JSON codec so they can travel over a ROS service.
//
// This is the server side's model. It is also what the unit test should use --
// CaliComputeTest needed QApplication and an offscreen platform only because the
// data lived in widgets.
//
// Storage is sparse per round (row -> col -> text). A round is 42x35 with maybe
// 400 cells filled on input, so a dense grid would be mostly empty strings and
// the JSON would be mostly commas.
class CaliTableData : public CaliDataSource {
public:
    // Geometry of a round table in the Cali Result form: 2 header rows + 75 layer
    // rows, and the 35 columns of _dict_column_index.
    //
    // 75, not 40. The form grows its tables to 77 rows in code (see "Ensure room
    // for all 75 layers" in controller_cali_result.cpp) and this is the headless
    // model of the SAME table, so a shorter default does not mean a smaller table
    // -- it means a truncated one. The after-bezel segment of a real capture
    // starts around layer 13 and runs past 40, and everything below the cut was
    // dropped without a message.
    static constexpr int kRows = 77;
    static constexpr int kCols = 35;

    CaliTableData() = default;

    // Give round `round` a backing table. Rounds without one report hasRound()
    // false, which is how "this tab does not exist" reaches the formulas.
    void addRound(int round, int rows = kRows, int cols = kCols);

    void setField(const QString &name, const QString &text) { fields_[name] = text; }
    QString fieldText(const QString &name) const { return fields_.value(name); }
    const QHash<QString, QString> &fields() const { return fields_; }

    // ---- CaliDataSource ----
    bool hasRound(int round) const override;
    int rowCount(int round) const override;
    int columnCount(int round) const override;
    QString cell(int round, int row, int col) const override;
    void setCell(int round, int row, int col, const QString &text) override;
    bool hasField(const QString &name) const override { return fields_.contains(name); }
    double field(const QString &name, double def) const override;
    void setFieldText(const QString &name, const QString &text) override { fields_[name] = text; }

    // ---- wire format -------------------------------------------------------
    // {"rounds": {"1": {"rows": 42, "cols": 35,
    //                   "cells": {"2": {"3": "100.5", ...}, ...}}, ...},
    //  "fields": {"lineedit_pixel_size_top": "0.2478", ...}}
    //
    // Cell values stay STRINGS end to end. The table is the data model and its
    // cells hold text: "" and "0" mean different things to every formula here
    // (avgWithoutZero counts one and not the other), and a JSON number would also
    // reformat the text the operator typed. Round-tripping the text is the only
    // way the server's answer is the same as the client's.
    QString toJson() const;
    static CaliTableData fromJson(const QString &json, bool *ok = nullptr);

private:
    struct Round {
        bool present = false;
        int rows = 0;
        int cols = 0;
        QHash<int, QHash<int, QString>> cells;  // row -> col -> text
    };
    Round rounds_[11];
    QHash<QString, QString> fields_;

    static bool validRound(int r) { return r >= 0 && r <= 10; }
};
