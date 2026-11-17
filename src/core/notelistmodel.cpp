// SPDX-License-Identifier: MIT
#include "notelistmodel.h"
#include "note.h"
#include <QSet>
#include <algorithm>
#include <vector>

NoteListModel::NoteListModel(QObject *parent) : QAbstractListModel(parent) {}
int NoteListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}
QVariant NoteListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0 || index.row() >= count()) return {};
    Note *note = m_notes.at(index.row());
    if (role == NoteRole) return QVariant::fromValue(note);
    if (role == IdRole) return note->id();
    return {};
}
QHash<int, QByteArray> NoteListModel::roleNames() const
{
    return {{NoteRole, "note"}, {IdRole, "noteId"}};
}
Note *NoteListModel::get(int row) const
{
    return row >= 0 && row < count() ? m_notes.at(row) : nullptr;
}
int NoteListModel::indexOf(Note *note) const { return m_notes.indexOf(note); }

void NoteListModel::setNotes(const QList<Note *> &notes)
{
    if (m_notes == notes) return;
    const auto oldCount = count();
    const QSet<Note *> wanted(notes.begin(), notes.end());
    for (int i = count() - 1; i >= 0;) {
        if (wanted.contains(m_notes.at(i))) { --i; continue; }
        const int last = i;
        while (i >= 0 && !wanted.contains(m_notes.at(i))) --i;
        beginRemoveRows({}, i + 1, last);
        m_notes.remove(i + 1, last - i);
        endRemoveRows();
    }
    if (m_notes.isEmpty() && !notes.isEmpty()) {
        beginInsertRows({}, 0, notes.size() - 1);
        m_notes = notes;
        endInsertRows();
    } else {
        // Retain a longest increasing subsequence: all other retained rows require exactly one move.
        QHash<Note *, int> target;
        for (int i = 0; i < notes.size(); ++i) target.insert(notes.at(i), i);
        std::vector<int> tails, previous(m_notes.size(), -1);
        for (int i = 0; i < m_notes.size(); ++i) {
            auto it = std::lower_bound(tails.begin(), tails.end(), target.value(m_notes.at(i)),
                [&](int row, int value) { return target.value(m_notes.at(row)) < value; });
            const auto slot = it - tails.begin();
            if (slot) previous[i] = tails[slot - 1];
            if (it == tails.end()) tails.push_back(i); else *it = i;
        }
        QSet<Note *> retained;
        if (!tails.empty())
            for (int i = tails.back(); i >= 0; i = previous[i]) retained.insert(m_notes.at(i));
        Note *anchor = nullptr;
        for (int i = notes.size() - 1; i >= 0; --i) {
            Note *note = notes.at(i);
            if (!retained.contains(note)) {
                const int from = m_notes.indexOf(note);
                const int before = anchor ? m_notes.indexOf(anchor) : count();
                if (from < 0) {
                    beginInsertRows({}, before, before);
                    m_notes.insert(before, note);
                    endInsertRows();
                } else if (from != before && from + 1 != before) {
                    beginMoveRows({}, from, from, {}, before);
                    m_notes.move(from, before > from ? before - 1 : before);
                    endMoveRows();
                }
            }
            anchor = note;
        }
    }
    if (oldCount != count()) Q_EMIT countChanged();
}
