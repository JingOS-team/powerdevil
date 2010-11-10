/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/


#ifndef POWERDEVILCORE_H
#define POWERDEVILCORE_H

#include "powerdevilbackendinterface.h"

#include <QtCore/QWeakPointer>
#include <QtCore/QStringList>

#include <KComponentData>

class QTimer;
class KNotification;
namespace Solid {
class Battery;
}

namespace PowerDevil
{

class BackendInterface;
class Action;

class KDE_EXPORT Core : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Core)
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement")

public:
    explicit Core(QObject* parent, const KComponentData &componentData);
    virtual ~Core();

    void reloadProfile(int state);

    void emitNotification(const QString &evid, const QString &message = QString(),
                          const QString &iconname = "dialog-ok-apply");

    BackendInterface *backend();
    
    // More...

public Q_SLOTS:
    // Set of common action - useful for the DBus interface
    void refreshStatus();
    void reloadProfile();
    void reloadCurrentProfile();

    void loadProfile(const QString &name);
    QString currentProfile() const;

    int brightness() const;
    void setBrightness(int percent);

    qulonglong batteryRemainingTime() const;

    void increaseBrightness();
    void decreaseBrightness();

    void suspendToRam();
    void suspendToDisk();
    void suspendHybrid();

    void onResumeFromSuspend();

Q_SIGNALS:
    void profileChanged(const QString &newProfile);
    void brightnessChanged(int percent);
    void batteryRemainingTimeChanged(qulonglong time);
    void resumingFromSuspend();

private:
    void registerActionTimeout(Action *action, int timeout);
    void unregisterActionTimeouts(Action *action);

    void triggerSuspendSession(uint action);

    friend class Action;

    BackendInterface *m_backend;
    QStringList m_loadedBatteriesUdi;

    QWeakPointer< KNotification > notification;
    KComponentData m_applicationData;
    KSharedConfigPtr m_profilesConfig;

    QString m_currentProfile;

    QHash< QString, int > m_batteriesPercent;

    QTimer *m_criticalBatteryTimer;

    // Idle time management
    QHash< int, int > m_registeredIdleTimeouts;
    QHash< Action*, QList< int > > m_registeredActionTimeouts;
    QList< Action* > m_pendingResumeFromIdleActions;

private Q_SLOTS:
    void onBackendReady();
    void onBackendError(const QString &error);
    void onAcAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState);
    void onBatteryChargePercentChanged(int,const QString&);
    void onBrightnessChanged(float);
    void onBatteryRemainingTimeChanged(qulonglong);
    void onKIdleTimeoutReached(int,int);
    void onResumingFromIdle();
    void onDeviceAdded(const QString &udi);
    void onDeviceRemoved(const QString &udi);
    void onCriticalBatteryTimerExpired();
    void checkBatteryStatus();
};

}

#endif // POWERDEVILCORE_H
