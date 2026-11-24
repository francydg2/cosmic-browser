#pragma once

#include <QDialog>

class AiService;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;

/// Provider, model, language and API key of the AI assistant. The key
/// goes to the encrypted vault, never to settings.json.
class AiSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    AiSettingsDialog(AiService *service, QWidget *parent = nullptr);

private:
    void providerChanged();
    void updateKeyStatus();
    void save();

    AiService *m_service;
    QComboBox *m_provider;
    QLineEdit *m_baseUrl;
    QLineEdit *m_model;
    QLineEdit *m_key;
    QLineEdit *m_language;
    QSpinBox *m_timeout;
    QLabel *m_keyStatus;
    QString m_previousDefaultUrl;
    QString m_previousDefaultModel;
};
