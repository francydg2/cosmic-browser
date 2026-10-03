#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

struct VaultEntry
{
    QString id;
    QString site;
    QString username;
    QString password;
    QString note;
    QString type;
    qint64 createdMs = 0;
    qint64 updatedMs = 0;
};

class PasswordVault : public QObject
{
    Q_OBJECT

public:
    explicit PasswordVault(const QString &filePath, QObject *parent = nullptr);

    QString filePath() const { return m_path; }
    bool fileExists() const;
    bool isUnlocked() const { return m_unlocked; }

    bool initialize(const QString &masterPassword);
    bool unlock(const QString &masterPassword);
    void lock();
    bool changeMasterPassword(const QString &current, const QString &neu);

    QVector<VaultEntry> entries() const;
    VaultEntry entry(const QString &id) const;
    QString addEntry(VaultEntry entryData);
    bool updateEntry(const VaultEntry &updated);
    bool removeEntry(const QString &id);

    VaultEntry entryForUrl(const QUrl &url) const;

    QString lastError() const { return m_lastError; }

signals:
    void lockStateChanged(bool unlocked);
    void entriesChanged();

private:
    bool persist();
    bool decryptFile(const QString &password, QByteArray *plainOut,
                     QByteArray *saltOut, QString *errorOut);
    bool encryptAndWrite(const QByteArray &plain, const QByteArray &key,
                         const QByteArray &salt, QString *errorOut);
    static QByteArray deriveKey(const QString &password, const QByteArray &salt);
    static QByteArray randomBytes(int count);
    static QByteArray aesGcmEncrypt(const QByteArray &plain, const QByteArray &key,
                                    const QByteArray &iv, QByteArray *tagOut);
    static QByteArray aesGcmDecrypt(const QByteArray &cipher, const QByteArray &key,
                                    const QByteArray &iv, const QByteArray &tag,
                                    bool *okOut);
    static QByteArray serializeEntries(const QVector<VaultEntry> &entries);
    static QVector<VaultEntry> parseEntries(const QByteArray &json);
    void setError(const QString &error);

    QString m_path;
    QString m_lastError;
    QByteArray m_key;
    QByteArray m_salt;
    QVector<VaultEntry> m_entries;
    bool m_unlocked = false;
};
