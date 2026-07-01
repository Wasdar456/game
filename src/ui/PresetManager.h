#ifndef PRESETMANAGER_H
#define PRESETMANAGER_H

#include "core/base/CoreTypes.h"

#include <QDateTime>
#include <QString>
#include <QVector>

struct DeckPreset {
    QString id;
    QString displayName;
    QVector<game::core::CardKind> cards;
    QDateTime updatedAt;
};

class PresetManager
{
public:
    PresetManager();

    QVector<DeckPreset> listPresets() const;
    DeckPreset defaultPreset() const;
    bool savePreset(const QString& displayName,
                    const QVector<game::core::CardKind>& cards,
                    QString* presetId = nullptr,
                    QString* error = nullptr);
    bool renamePreset(const QString& id, const QString& displayName, QString* error = nullptr);
    bool deletePreset(const QString& id, QString* error = nullptr);
    bool loadPreset(const QString& id, DeckPreset& preset) const;
    QString storagePath() const { return m_storagePath; }

private:
    QString m_storagePath;
    QVector<DeckPreset> m_presets;

    void load();
    bool save(QString* error = nullptr) const;
    int indexForId(const QString& id) const;
};

#endif // PRESETMANAGER_H
