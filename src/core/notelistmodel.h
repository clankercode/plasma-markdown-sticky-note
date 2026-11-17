// SPDX-License-Identifier: MIT
#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QtQml/qqmlregistration.h>

class Note;

// A lightweight list model over a subset of Note objects. The NoteStore decides membership and order
// and calls setNotes(); the model computes a minimal diff (insert/remove/move rows) so QML delegates
// are NOT recreated for notes that stay in the list. Per-note property changes are observed by QML
// through the Note object itself (role "note"), so the model does not emit dataChanged for them.
class NoteListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by NoteStore")
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles { NoteRole = Qt::UserRole + 1, IdRole };

    explicit NoteListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override; // "note", "noteId"

    int count() const { return m_notes.size(); }
    QList<Note *> notes() const { return m_notes; }

    void setNotes(const QList<Note *> &notes);

    Q_INVOKABLE Note *get(int row) const;      // nullptr if out of range
    Q_INVOKABLE int indexOf(Note *note) const; // -1 if absent

Q_SIGNALS:
    void countChanged();

private:
    QList<Note *> m_notes;
};
