#pragma once

#include "core/UILayout.h"

#include <QObject>
#include <QString>

class LayoutEditor : public QObject
{
    Q_OBJECT

public:
    explicit LayoutEditor(QObject *parent = nullptr);

    void setConfiguration(const UIConfiguration &cfg);
    UIConfiguration configuration() const { return m_working; }
    bool isModified() const;

    bool moveItem(int fromZone, int fromIndex, int toZone, int toIndex);
    bool addItem(int zone, int index, const QString &component);
    bool removeItem(int zone, int index);
    bool setIconSize(int size);
    bool setSpacing(int spacing);
    bool setToolbarHeight(int height);
    bool setToolbarColor(const QString &color);
    bool setUrlHeight(int height);
    bool setUrlRadius(int radius);
    bool setUrlMaxWidth(int maxWidth);
    bool setUrlColor(const QString &color);
    bool setTabPosition(const QString &position);
    bool setTabHeight(int height);
    bool setTabRadius(int radius);
    bool setTabSpacing(int spacing);
    bool setShowPlus(bool show);

    static QString zoneName(int zone);

signals:
    void changed();
    void error(const QString &message);

private:
    QList<UiToolbarItem> *zoneList(int zone);
    const QList<UiToolbarItem> *zoneList(int zone) const;

    UIConfiguration m_working;
    UIConfiguration m_base;
};
