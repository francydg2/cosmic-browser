#include "ui/AiSettingsDialog.h"

#include "browser/AiService.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

AiSettingsDialog::AiSettingsDialog(AiService *service, QWidget *parent)
    : QDialog(parent)
    , m_service(service)
    , m_provider(new QComboBox(this))
    , m_baseUrl(new QLineEdit(this))
    , m_model(new QLineEdit(this))
    , m_key(new QLineEdit(this))
    , m_language(new QLineEdit(this))
    , m_timeout(new QSpinBox(this))
    , m_keyStatus(new QLabel(this))
{
    setObjectName(QStringLiteral("aiSettingsDialog"));
    setWindowTitle(tr("AI assistant settings"));
    resize(520, 0);
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    for (const QString &id : Ai::kindIds()) {
        Ai::Kind kind;
        Ai::kindFromId(id, &kind);
        m_provider->addItem(Ai::kindLabel(kind), id);
    }
    m_provider->setObjectName(QStringLiteral("aiProvider"));
    m_baseUrl->setObjectName(QStringLiteral("aiBaseUrl"));
    m_model->setObjectName(QStringLiteral("aiModel"));
    m_key->setObjectName(QStringLiteral("aiKey"));
    m_key->setEchoMode(QLineEdit::Password);
    m_key->setPlaceholderText(tr("Leave empty to keep the stored key"));
    m_language->setPlaceholderText(tr("Same language as the question"));
    m_timeout->setRange(5, 600);
    m_timeout->setSuffix(tr(" s"));
    form->addRow(tr("Provider"), m_provider);
    form->addRow(tr("Address"), m_baseUrl);
    form->addRow(tr("Model"), m_model);
    form->addRow(tr("API key"), m_key);
    form->addRow(QString(), m_keyStatus);
    form->addRow(tr("Answer language"), m_language);
    form->addRow(tr("Inactivity timeout"), m_timeout);
    layout->addLayout(form);

    auto *notice = new QLabel(
        tr("Keys are stored in Cosmic's encrypted password vault (or read from the environment "
           "variable shown above) and are never written to the settings file or logs. Page "
           "text is sent only when you ask, after you confirm, unless the provider runs on this "
           "computer."), this);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    auto *remove = new QPushButton(tr("Remove stored key"), this);
    remove->setObjectName(QStringLiteral("aiRemoveKey"));
    layout->addWidget(remove, 0, Qt::AlignLeft);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    layout->addWidget(box);

    const Ai::ProviderConfig config = m_service->configWithoutKey();
    m_provider->setCurrentIndex(m_provider->findData(Ai::kindId(config.kind)));
    m_previousDefaultUrl = Ai::defaultBaseUrl(config.kind);
    m_previousDefaultModel = Ai::defaultModel(config.kind);
    m_baseUrl->setText(config.baseUrl);
    m_model->setText(config.model);
    m_timeout->setValue(config.timeoutSeconds);
    const QString language = m_service->language();
    m_language->setText(language.startsWith(QLatin1String("the user")) ? QString() : language);
    updateKeyStatus();

    connect(m_provider, &QComboBox::currentIndexChanged, this, &AiSettingsDialog::providerChanged);
    connect(remove, &QPushButton::clicked, this, [this] {
        Ai::Kind kind;
        Ai::kindFromId(m_provider->currentData().toString(), &kind);
        if (!m_service->removeKey(kind)) {
            QMessageBox::information(this, tr("API key"),
                tr("No key is stored in the vault, or the vault is locked."));
        }
        updateKeyStatus();
    });
    connect(box, &QDialogButtonBox::accepted, this, &AiSettingsDialog::save);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void AiSettingsDialog::providerChanged()
{
    Ai::Kind kind;
    Ai::kindFromId(m_provider->currentData().toString(), &kind);
    // Replace untouched defaults only, never text the user typed.
    if (m_baseUrl->text().isEmpty() || m_baseUrl->text() == m_previousDefaultUrl) {
        m_baseUrl->setText(Ai::defaultBaseUrl(kind));
    }
    if (m_model->text().isEmpty() || m_model->text() == m_previousDefaultModel) {
        m_model->setText(Ai::defaultModel(kind));
    }
    m_previousDefaultUrl = Ai::defaultBaseUrl(kind);
    m_previousDefaultModel = Ai::defaultModel(kind);
    updateKeyStatus();
}

void AiSettingsDialog::updateKeyStatus()
{
    Ai::Kind kind;
    Ai::kindFromId(m_provider->currentData().toString(), &kind);
    QString text;
    switch (m_service->keySource(kind)) {
    case AiService::KeySource::Vault:
        text = tr("A key is stored in the encrypted vault.");
        break;
    case AiService::KeySource::Environment:
        text = tr("Using the key from the environment variable %1.")
                   .arg(Ai::keyEnvironmentVariable(kind));
        break;
    case AiService::KeySource::VaultLocked:
        text = tr("The vault is locked: unlock it in Settings → Passwords to use or save a key.");
        break;
    case AiService::KeySource::None:
        text = Ai::requiresKey(kind) ? tr("No key stored. Environment variable: %1.")
                                           .arg(Ai::keyEnvironmentVariable(kind))
                                     : tr("This provider does not need a key.");
        break;
    }
    m_keyStatus->setText(text);
}

void AiSettingsDialog::save()
{
    Ai::ProviderConfig config;
    Ai::kindFromId(m_provider->currentData().toString(), &config.kind);
    config.baseUrl = m_baseUrl->text().trimmed();
    config.model = m_model->text().trimmed();
    config.timeoutSeconds = m_timeout->value();
    Ai::ProviderConfig check = config;
    check.apiKey = QStringLiteral("x"); // key presence is validated at send time
    const QString problem = Ai::validate(check);
    if (!problem.isEmpty()) {
        QMessageBox::warning(this, tr("AI assistant settings"), problem);
        return;
    }
    if (!m_key->text().trimmed().isEmpty()) {
        QString error;
        if (!m_service->storeKey(config.kind, m_key->text(), &error)) {
            QMessageBox::warning(this, tr("API key"), error);
            return;
        }
        m_key->clear();
    }
    m_service->setConfig(config, m_language->text());
    accept();
}
