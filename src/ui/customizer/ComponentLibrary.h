#pragma once

#include <QList>
#include <QString>
#include <QTreeWidget>

struct ComponentInfo {
    QString id;
    QString label;
    QString description;
    QString iconPath;
    QString category;
    bool available = false;
    bool toolbarItem = false;
    bool unique = false;
};

class ComponentLibrary : public QTreeWidget
{
    Q_OBJECT

public:
    explicit ComponentLibrary(QWidget *parent = nullptr);

    static QList<ComponentInfo> components();
    static const ComponentInfo *infoFor(const QString &id);
    static QString iconFor(const QString &id);
    static QString labelFor(const QString &id);
    static bool isToolbarItem(const QString &id);

signals:
    void addRequested(const QString &component);

protected:
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override;

private:
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
};
