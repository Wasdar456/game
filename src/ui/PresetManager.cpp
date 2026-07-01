#include "ui/PresetManager.h"

#include "core/data/CardSpecs.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>

namespace {

QString cardKindToKey(game::core::CardKind kind)
{
    return QString::fromUtf8(game::core::cardKey(kind));
}

bool keyToCardKind(const QString& key, game::core::CardKind& kind)
{
    for (const auto& spec : game::core::kCardSpecs) {
        if (key == QString::fromUtf8(spec.key)) {
            kind = spec.kind;
            return true;
        }
    }
    return false;
}

QString defaultStoragePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        dir = QDir::home().filePath(".dffense-and-attack");
    }
    QDir().mkpath(dir);
    return QDir(dir).filePath("deck_presets.json");
}

QVector<game::core::CardKind> normalizedDeck(const QVector<game::core::CardKind>& cards)
{
    QVector<game::core::CardKind> result;
    result.reserve(5);
    for (game::core::CardKind kind : cards) {
        if (result.size() >= 5) break;
        result.append(kind);
    }
    return result;
}

} // namespace

PresetManager::PresetManager()
    : m_storagePath(defaultStoragePath())
{
    load();
}

DeckPreset PresetManager::defaultPreset() const
{
    DeckPreset preset;
    preset.id = "default";
    preset.displayName = "Default Squad";
    preset.cards = {
        game::core::CardKind::Produce,
        game::core::CardKind::Attack,
        game::core::CardKind::Sniper,
        game::core::CardKind::Heal,
        game::core::CardKind::Aoe
    };
    preset.updatedAt = QDateTime::currentDateTimeUtc();
    return preset;
}

QVector<DeckPreset> PresetManager::listPresets() const
{
    QVector<DeckPreset> result;
    result.reserve(m_presets.size() + 1);
    result.append(defaultPreset());
    for (const auto& preset : m_presets) {
        result.append(preset);
    }
    return result;
}

bool PresetManager::savePreset(const QString& displayName,
                               const QVector<game::core::CardKind>& cards,
                               QString* presetId,
                               QString* error)
{
    const QString name = displayName.trimmed();
    if (name.isEmpty()) {
        if (error) *error = "Preset name cannot be empty.";
        return false;
    }
    const QVector<game::core::CardKind> deck = normalizedDeck(cards);
    if (deck.isEmpty()) {
        if (error) *error = "Preset must contain at least one card.";
        return false;
    }

    DeckPreset preset;
    preset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    preset.displayName = name;
    preset.cards = deck;
    preset.updatedAt = QDateTime::currentDateTimeUtc();
    m_presets.append(preset);
    if (presetId) *presetId = preset.id;
    return save(error);
}

bool PresetManager::renamePreset(const QString& id, const QString& displayName, QString* error)
{
    if (id == "default") {
        if (error) *error = "The built-in preset cannot be renamed.";
        return false;
    }
    const int index = indexForId(id);
    if (index < 0) {
        if (error) *error = "Preset not found.";
        return false;
    }
    const QString name = displayName.trimmed();
    if (name.isEmpty()) {
        if (error) *error = "Preset name cannot be empty.";
        return false;
    }
    m_presets[index].displayName = name;
    m_presets[index].updatedAt = QDateTime::currentDateTimeUtc();
    return save(error);
}

bool PresetManager::deletePreset(const QString& id, QString* error)
{
    if (id == "default") {
        if (error) *error = "The built-in preset cannot be deleted.";
        return false;
    }
    const int index = indexForId(id);
    if (index < 0) {
        if (error) *error = "Preset not found.";
        return false;
    }
    m_presets.removeAt(index);
    return save(error);
}

bool PresetManager::loadPreset(const QString& id, DeckPreset& preset) const
{
    if (id == "default") {
        preset = defaultPreset();
        return true;
    }
    const int index = indexForId(id);
    if (index < 0) return false;
    preset = m_presets[index];
    return true;
}

void PresetManager::load()
{
    m_presets.clear();
    QFile file(m_storagePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return;
    }
    const QJsonArray presets = document.object().value("presets").toArray();
    for (const QJsonValue& value : presets) {
        if (!value.isObject()) continue;
        const QJsonObject object = value.toObject();
        DeckPreset preset;
        preset.id = object.value("id").toString().trimmed();
        preset.displayName = object.value("displayName").toString().trimmed();
        preset.updatedAt = QDateTime::fromString(object.value("updatedAt").toString(), Qt::ISODate);
        const QJsonArray cardArray = object.value("cards").toArray();
        for (const QJsonValue& cardValue : cardArray) {
            game::core::CardKind kind = game::core::CardKind::Attack;
            if (keyToCardKind(cardValue.toString(), kind) && preset.cards.size() < 5) {
                preset.cards.append(kind);
            }
        }
        if (!preset.id.isEmpty() && !preset.displayName.isEmpty() && !preset.cards.isEmpty()) {
            m_presets.append(preset);
        }
    }
}

bool PresetManager::save(QString* error) const
{
    QFileInfo info(m_storagePath);
    QDir().mkpath(info.absolutePath());

    QJsonArray presets;
    for (const auto& preset : m_presets) {
        QJsonArray cards;
        for (game::core::CardKind kind : preset.cards) {
            cards.append(cardKindToKey(kind));
        }
        QJsonObject object;
        object["id"] = preset.id;
        object["displayName"] = preset.displayName;
        object["updatedAt"] = preset.updatedAt.toUTC().toString(Qt::ISODate);
        object["cards"] = cards;
        presets.append(object);
    }

    QJsonObject root;
    root["schemaVersion"] = 1;
    root["presets"] = presets;

    QFile file(m_storagePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

int PresetManager::indexForId(const QString& id) const
{
    for (int i = 0; i < m_presets.size(); ++i) {
        if (m_presets[i].id == id) return i;
    }
    return -1;
}
