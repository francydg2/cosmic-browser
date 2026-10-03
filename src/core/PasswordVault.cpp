#include "core/PasswordVault.h"
#include "core/Logging.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#include <openssl/evp.h>
#include <openssl/rand.h>

namespace {

const char kMagic[8] = {'N', 'E', 'B', 'V', 'A', 'U', 'L', 'T'};
constexpr int kMagicLen = 8;
constexpr int kSaltLen = 16;
constexpr int kIvLen = 12;
constexpr int kTagLen = 16;
constexpr int kKeyLen = 32;
constexpr int kPbkdf2Iterations = 210000;
constexpr int kHeaderLen = kMagicLen + kSaltLen + kIvLen + kTagLen;

const auto kKeyId = "id";
const auto kKeySite = "site";
const auto kKeyUsername = "username";
const auto kKeyPassword = "password";
const auto kKeyNote = "note";
const auto kKeyType = "type";
const auto kKeyCreated = "created";
const auto kKeyUpdated = "updated";

} // namespace

PasswordVault::PasswordVault(const QString &filePath, QObject *parent)
    : QObject(parent)
    , m_path(filePath)
{
}

bool PasswordVault::fileExists() const
{
    QFile file(m_path);
    return file.exists() && file.size() >= kHeaderLen;
}

void PasswordVault::setError(const QString &error)
{
    m_lastError = error;
    if (!error.isEmpty()) {
        qCWarning(lcCosmic).noquote() << "vault:" << error;
    }
}

QByteArray PasswordVault::randomBytes(int count)
{
    QByteArray data(count, Qt::Uninitialized);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(data.data()), count) != 1) {
        return {};
    }
    return data;
}

QByteArray PasswordVault::deriveKey(const QString &password, const QByteArray &salt)
{
    QByteArray key(kKeyLen, Qt::Uninitialized);
    const QByteArray pass = password.toUtf8();
    if (PKCS5_PBKDF2_HMAC(pass.constData(), pass.size(),
                          reinterpret_cast<const unsigned char *>(salt.constData()),
                          salt.size(), kPbkdf2Iterations, EVP_sha256(), kKeyLen,
                          reinterpret_cast<unsigned char *>(key.data()))
        != 1) {
        return {};
    }
    return key;
}

QByteArray PasswordVault::aesGcmEncrypt(const QByteArray &plain, const QByteArray &key,
                                        const QByteArray &iv, QByteArray *tagOut)
{
    QByteArray cipher(plain.size() + 16, Qt::Uninitialized);
    QByteArray tag(kTagLen, Qt::Uninitialized);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return {};
    }
    int len = 0;
    int total = 0;
    bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) == 1
        && EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                              reinterpret_cast<const unsigned char *>(key.constData()),
                              reinterpret_cast<const unsigned char *>(iv.constData())) == 1
        && EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char *>(cipher.data()), &len,
                             reinterpret_cast<const unsigned char *>(plain.constData()),
                             plain.size()) == 1;
    total = len;
    if (ok) {
        ok = EVP_EncryptFinal_ex(ctx,
                                 reinterpret_cast<unsigned char *>(cipher.data()) + total,
                                 &len) == 1;
        total += len;
    }
    if (ok) {
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagLen,
                                 tag.data()) == 1;
    }
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) {
        return {};
    }
    cipher.truncate(total);
    *tagOut = tag;
    return cipher;
}

QByteArray PasswordVault::aesGcmDecrypt(const QByteArray &cipher, const QByteArray &key,
                                        const QByteArray &iv, const QByteArray &tag,
                                        bool *okOut)
{
    QByteArray plain(cipher.size() + 16, Qt::Uninitialized);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        *okOut = false;
        return {};
    }
    int len = 0;
    int total = 0;
    bool ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) == 1
        && EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                              reinterpret_cast<const unsigned char *>(key.constData()),
                              reinterpret_cast<const unsigned char *>(iv.constData())) == 1
        && EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char *>(plain.data()), &len,
                             reinterpret_cast<const unsigned char *>(cipher.constData()),
                             cipher.size()) == 1;
    total = len;
    if (ok) {
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagLen,
                                 const_cast<char *>(tag.constData())) == 1
            && EVP_DecryptFinal_ex(ctx,
                                   reinterpret_cast<unsigned char *>(plain.data()) + total,
                                   &len) == 1;
        total += len;
    }
    EVP_CIPHER_CTX_free(ctx);
    *okOut = ok;
    if (!ok) {
        return {};
    }
    plain.truncate(total);
    return plain;
}

QByteArray PasswordVault::serializeEntries(const QVector<VaultEntry> &entries)
{
    QJsonArray array;
    for (const VaultEntry &entryData : entries) {
        QJsonObject obj;
        obj.insert(QString::fromLatin1(kKeyId), entryData.id);
        obj.insert(QString::fromLatin1(kKeySite), entryData.site);
        obj.insert(QString::fromLatin1(kKeyUsername), entryData.username);
        obj.insert(QString::fromLatin1(kKeyPassword), entryData.password);
        obj.insert(QString::fromLatin1(kKeyNote), entryData.note);
        obj.insert(QString::fromLatin1(kKeyType),
                   entryData.type.isEmpty() ? QStringLiteral("password") : entryData.type);
        obj.insert(QString::fromLatin1(kKeyCreated), entryData.createdMs);
        obj.insert(QString::fromLatin1(kKeyUpdated), entryData.updatedMs);
        array.append(obj);
    }
    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

QVector<VaultEntry> PasswordVault::parseEntries(const QByteArray &json)
{
    QVector<VaultEntry> result;
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray()) {
        return result;
    }
    for (const QJsonValue &value : doc.array()) {
        const QJsonObject obj = value.toObject();
        VaultEntry entryData;
        entryData.id = obj.value(QString::fromLatin1(kKeyId)).toString();
        entryData.site = obj.value(QString::fromLatin1(kKeySite)).toString();
        entryData.username = obj.value(QString::fromLatin1(kKeyUsername)).toString();
        entryData.password = obj.value(QString::fromLatin1(kKeyPassword)).toString();
        entryData.note = obj.value(QString::fromLatin1(kKeyNote)).toString();
        entryData.type = obj.value(QString::fromLatin1(kKeyType)).toString();
        entryData.createdMs = static_cast<qint64>(obj.value(QString::fromLatin1(kKeyCreated)).toDouble());
        entryData.updatedMs = static_cast<qint64>(obj.value(QString::fromLatin1(kKeyUpdated)).toDouble());
        if (entryData.id.isEmpty()) {
            continue;
        }
        result.append(entryData);
    }
    return result;
}

bool PasswordVault::encryptAndWrite(const QByteArray &plain, const QByteArray &key,
                                    const QByteArray &salt, QString *errorOut)
{
    const QByteArray iv = randomBytes(kIvLen);
    if (iv.size() != kIvLen || key.size() != kKeyLen) {
        *errorOut = QStringLiteral("crypto initialization failed");
        return false;
    }
    QByteArray tag;
    const QByteArray cipher = aesGcmEncrypt(plain, key, iv, &tag);
    if (cipher.isNull() || tag.size() != kTagLen) {
        *errorOut = QStringLiteral("encryption failed");
        return false;
    }

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        *errorOut = QStringLiteral("cannot write vault file");
        return false;
    }
    file.write(kMagic, kMagicLen);
    file.write(salt);
    file.write(iv);
    file.write(tag);
    file.write(cipher);
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (!file.commit()) {
        *errorOut = QStringLiteral("failed to commit vault file");
        return false;
    }
    return true;
}

bool PasswordVault::decryptFile(const QString &password, QByteArray *plainOut,
                                QByteArray *saltOut, QString *errorOut)
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        *errorOut = QStringLiteral("cannot read vault file");
        return false;
    }
    const QByteArray data = file.readAll();
    if (data.size() < kHeaderLen) {
        *errorOut = QStringLiteral("vault file corrupt");
        return false;
    }
    if (memcmp(data.constData(), kMagic, kMagicLen) != 0) {
        *errorOut = QStringLiteral("vault file corrupt");
        return false;
    }
    const QByteArray salt = data.mid(kMagicLen, kSaltLen);
    const QByteArray iv = data.mid(kMagicLen + kSaltLen, kIvLen);
    const QByteArray tag = data.mid(kMagicLen + kSaltLen + kIvLen, kTagLen);
    const QByteArray cipher = data.mid(kHeaderLen);

    const QByteArray key = deriveKey(password, salt);
    if (key.size() != kKeyLen) {
        *errorOut = QStringLiteral("crypto initialization failed");
        return false;
    }
    bool ok = false;
    const QByteArray plain = aesGcmDecrypt(cipher, key, iv, tag, &ok);
    if (!ok) {
        *errorOut = QStringLiteral("wrong master password");
        return false;
    }
    *plainOut = plain;
    *saltOut = salt;
    return true;
}

bool PasswordVault::initialize(const QString &masterPassword)
{
    if (masterPassword.size() < 4) {
        setError(QStringLiteral("master password too short (min 4)"));
        return false;
    }
    if (fileExists()) {
        setError(QStringLiteral("vault already initialized"));
        return false;
    }
    const QByteArray salt = randomBytes(kSaltLen);
    const QByteArray key = deriveKey(masterPassword, salt);
    if (salt.size() != kSaltLen || key.size() != kKeyLen) {
        setError(QStringLiteral("crypto initialization failed"));
        return false;
    }
    QString error;
    if (!encryptAndWrite(QByteArrayLiteral("[]"), key, salt, &error)) {
        setError(error);
        return false;
    }
    m_salt = salt;
    m_key = key;
    m_entries.clear();
    m_unlocked = true;
    setError({});
    emit lockStateChanged(true);
    emit entriesChanged();
    qCInfo(lcCosmic).noquote() << "vault initialized at" << m_path;
    return true;
}

bool PasswordVault::unlock(const QString &masterPassword)
{
    if (m_unlocked) {
        return true;
    }
    if (!fileExists()) {
        setError(QStringLiteral("vault not initialized"));
        return false;
    }
    QByteArray plain;
    QByteArray salt;
    QString error;
    if (!decryptFile(masterPassword, &plain, &salt, &error)) {
        setError(error);
        return false;
    }
    m_key = deriveKey(masterPassword, salt);
    if (m_key.size() != kKeyLen) {
        setError(QStringLiteral("crypto initialization failed"));
        return false;
    }
    m_salt = salt;
    m_entries = parseEntries(plain);
    m_unlocked = true;
    setError({});
    emit lockStateChanged(true);
    emit entriesChanged();
    qCInfo(lcCosmic).noquote() << "vault unlocked:" << m_entries.size() << "entries";
    return true;
}

void PasswordVault::lock()
{
    if (!m_unlocked) {
        return;
    }
    m_unlocked = false;
    m_key.fill(0);
    m_entries.clear();
    emit lockStateChanged(false);
    emit entriesChanged();
}

bool PasswordVault::changeMasterPassword(const QString &current, const QString &neu)
{
    if (neu.size() < 4) {
        setError(QStringLiteral("master password too short (min 4)"));
        return false;
    }
    QByteArray plain;
    QByteArray salt;
    QString error;
    if (!decryptFile(current, &plain, &salt, &error)) {
        setError(error);
        return false;
    }
    const QByteArray newSalt = randomBytes(kSaltLen);
    const QByteArray newKey = deriveKey(neu, newSalt);
    if (newSalt.size() != kSaltLen || newKey.size() != kKeyLen) {
        setError(QStringLiteral("crypto initialization failed"));
        return false;
    }
    if (!encryptAndWrite(plain, newKey, newSalt, &error)) {
        setError(error);
        return false;
    }
    m_salt = newSalt;
    m_key = newKey;
    m_entries = parseEntries(plain);
    m_unlocked = true;
    setError({});
    emit lockStateChanged(true);
    emit entriesChanged();
    return true;
}

bool PasswordVault::persist()
{
    if (!m_unlocked) {
        setError(QStringLiteral("vault locked"));
        return false;
    }
    QString error;
    if (!encryptAndWrite(serializeEntries(m_entries), m_key, m_salt, &error)) {
        setError(error);
        return false;
    }
    return true;
}

QVector<VaultEntry> PasswordVault::entries() const
{
    return m_unlocked ? m_entries : QVector<VaultEntry>{};
}

VaultEntry PasswordVault::entry(const QString &id) const
{
    if (!m_unlocked) {
        return {};
    }
    for (const VaultEntry &entryData : m_entries) {
        if (entryData.id == id) {
            return entryData;
        }
    }
    return {};
}

QString PasswordVault::addEntry(VaultEntry entryData)
{
    if (!m_unlocked) {
        setError(QStringLiteral("vault locked"));
        return {};
    }
    if (entryData.site.trimmed().isEmpty()) {
        setError(QStringLiteral("site is required"));
        return {};
    }
    entryData.id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);
    if (entryData.type.isEmpty()) {
        entryData.type = QStringLiteral("password");
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    entryData.createdMs = now;
    entryData.updatedMs = now;
    m_entries.append(entryData);
    if (!persist()) {
        m_entries.removeLast();
        return {};
    }
    emit entriesChanged();
    return entryData.id;
}

bool PasswordVault::updateEntry(const VaultEntry &updated)
{
    if (!m_unlocked) {
        setError(QStringLiteral("vault locked"));
        return false;
    }
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == updated.id) {
            const VaultEntry before = m_entries.at(i);
            VaultEntry next = updated;
            next.createdMs = before.createdMs;
            next.updatedMs = QDateTime::currentMSecsSinceEpoch();
            m_entries[i] = next;
            if (!persist()) {
                m_entries[i] = before;
                return false;
            }
            emit entriesChanged();
            return true;
        }
    }
    setError(QStringLiteral("entry not found"));
    return false;
}

bool PasswordVault::removeEntry(const QString &id)
{
    if (!m_unlocked) {
        setError(QStringLiteral("vault locked"));
        return false;
    }
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == id) {
            const VaultEntry removed = m_entries.takeAt(i);
            if (!persist()) {
                m_entries.insert(i, removed);
                return false;
            }
            emit entriesChanged();
            return true;
        }
    }
    setError(QStringLiteral("entry not found"));
    return false;
}

static QString siteHost(const QString &site)
{
    QString value = site.trimmed();
    if (value.contains(QLatin1String("://"))) {
        value = QUrl(value).host();
    } else if (value.contains(QLatin1Char('/'))) {
        value = value.left(value.indexOf(QLatin1Char('/')));
    }
    return value.toLower();
}

VaultEntry PasswordVault::entryForUrl(const QUrl &url) const
{
    if (!m_unlocked) {
        return {};
    }
    const QString host = url.host().toLower();
    if (host.isEmpty()) {
        return {};
    }
    for (const VaultEntry &entryData : m_entries) {
        if (entryData.type != QLatin1String("password")) {
            continue;
        }
        const QString site = siteHost(entryData.site);
        if (site.isEmpty()) {
            continue;
        }
        if (host == site) {
            return entryData;
        }
    }
    for (const VaultEntry &entryData : m_entries) {
        if (entryData.type != QLatin1String("password")) {
            continue;
        }
        const QString site = siteHost(entryData.site);
        if (site.isEmpty()) {
            continue;
        }
        if (host.endsWith(QLatin1Char('.') + site)) {
            return entryData;
        }
    }
    return {};
}
