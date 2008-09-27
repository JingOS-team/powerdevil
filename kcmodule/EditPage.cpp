/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kdemod.ath.cx>                *
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

#include "EditPage.h"

#include "PowerDevilSettings.h"

#include <solid/control/powermanager.h>
#include <solid/device.h>
#include <solid/deviceinterface.h>
#include <solid/processor.h>

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusConnection>

#include <KStandardDirs>
#include <KRun>

#include <config-workspace.h>

#include <KConfigGroup>
#include <KLineEdit>
#include <QCheckBox>
#include <QFormLayout>
#include <KDialog>
#include <KFileDialog>
#include <KMessageBox>
#include <KIconButton>

EditPage::EditPage(QWidget *parent)
        : QWidget(parent),
        m_profileEdited(false)
{
    setupUi(this);

    m_profilesConfig = KSharedConfig::openConfig("powerdevilprofilesrc", KConfig::SimpleConfig);

    if (m_profilesConfig->groupList().isEmpty()) {
        // Let's add some basic profiles, huh?

        KConfigGroup *performance = new KConfigGroup(m_profilesConfig, "Performance");

        performance->writeEntry("brightness", 100);
        performance->writeEntry("cpuPolicy", (int) Solid::Control::PowerManager::Performance);
        performance->writeEntry("idleAction", 0);
        performance->writeEntry("idleTime", 50);
        performance->writeEntry("lidAction", 0);
        performance->writeEntry("turnOffIdle", false);
        performance->writeEntry("turnOffIdleTime", 120);

        performance->sync();

        kDebug() << performance->readEntry("brightness");

        delete performance;
    }

    fillUi();
}

EditPage::~EditPage()
{
}

void EditPage::fillUi()
{
    idleCombo->addItem(i18n("Do nothing"), (int) None);
    idleCombo->addItem(i18n("Shutdown"), (int) Shutdown);
    idleCombo->addItem(i18n("Lock Screen"), (int) Lock);
    laptopClosedCombo->addItem(i18n("Do nothing"), (int) None);
    laptopClosedCombo->addItem(i18n("Shutdown"), (int) Shutdown);
    laptopClosedCombo->addItem(i18n("Lock Screen"), (int) Lock);

    Solid::Control::PowerManager::SuspendMethods methods = Solid::Control::PowerManager::supportedSuspendMethods();

    Solid::Control::PowerManager::BrightnessControlsList bControls =
        Solid::Control::PowerManager::brightnessControlsAvailable();

    brightnessSlider->setEnabled(bControls.values().contains(Solid::Control::PowerManager::Screen));

    if (methods & Solid::Control::PowerManager::ToDisk) {
        idleCombo->addItem(i18n("Suspend to Disk"), (int) S2Disk);
        laptopClosedCombo->addItem(i18n("Suspend to Disk"), (int) S2Disk);
    }

    if (methods & Solid::Control::PowerManager::ToRam) {
        idleCombo->addItem(i18n("Suspend to Ram"), (int) S2Ram);
        laptopClosedCombo->addItem(i18n("Suspend to Ram"), (int) S2Ram);
    }

    if (methods & Solid::Control::PowerManager::Standby) {
        idleCombo->addItem(i18n("Standby"), (int) Standby);
        laptopClosedCombo->addItem(i18n("Standby"), (int) Standby);
    }

    Solid::Control::PowerManager::CpuFreqPolicies policies = Solid::Control::PowerManager::supportedCpuFreqPolicies();

    if (policies & Solid::Control::PowerManager::Performance) {
        freqCombo->addItem(i18n("Performance"), (int) Solid::Control::PowerManager::Performance);
    }

    if (policies & Solid::Control::PowerManager::OnDemand) {
        freqCombo->addItem(i18n("Dynamic (ondemand)"), (int) Solid::Control::PowerManager::OnDemand);
    }

    if (policies & Solid::Control::PowerManager::Conservative) {
        freqCombo->addItem(i18n("Dynamic (conservative)"), (int) Solid::Control::PowerManager::Conservative);
    }

    if (policies & Solid::Control::PowerManager::Powersave) {
        freqCombo->addItem(i18n("Powersave"), (int) Solid::Control::PowerManager::Powersave);
    }

    if (policies & Solid::Control::PowerManager::Userspace) {
        freqCombo->addItem(i18n("Userspace"), (int) Solid::Control::PowerManager::Userspace);
    }

    schemeCombo->addItems(Solid::Control::PowerManager::supportedSchemes());


    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Processor, QString())) {
        Solid::Device d = device;
        Solid::Processor *processor = qobject_cast<Solid::Processor*> (d.asDeviceInterface(Solid::DeviceInterface::Processor));

        QString text = i18n("CPU <numid>%1</numid>", processor->number());

        QCheckBox *checkBox = new QCheckBox(this);

        checkBox->setText(text);
        checkBox->setToolTip(i18n("Disable CPU <numid>%1</numid>", processor->number()));
        checkBox->setWhatsThis(i18n("If this box is checked, the CPU <numid>%1</numid> "
                                    "will be disabled", processor->number()));

        checkBox->setEnabled(Solid::Control::PowerManager::canDisableCpu(processor->number()));

        connect(checkBox, SIGNAL(stateChanged(int)), SLOT(emitChanged()));

        CPUListLayout->addWidget(checkBox);
    }

    reloadAvailableProfiles();

    newProfile->setIcon(KIcon("document-new"));
    editProfileButton->setIcon(KIcon("edit-rename"));
    deleteProfile->setIcon(KIcon("edit-delete-page"));
    importButton->setIcon(KIcon("document-import"));
    exportButton->setIcon(KIcon("document-export"));
    saveCurrentProfileButton->setIcon(KIcon("document-save"));
    resetCurrentProfileButton->setIcon(KIcon("edit-undo"));

    DPMSLabel->setUrl("http://www.energystar.gov");
    DPMSLabel->setPixmap(QPixmap(KStandardDirs::locate("data", "kcontrol/pics/energybig.png")));
    DPMSLabel->setTipText(i18n("Learn more about the Energy Star program"));
    DPMSLabel->setUseTips(true);
    connect(DPMSLabel, SIGNAL(leftClickedUrl(const QString&)), SLOT(openUrl(const QString &)));

#ifndef HAVE_DPMS
    DPMSEnable->setEnabled(false);
    DPMSSuspend->setEnabled(false);
    DPMSStandby->setEnabled(false);
    DPMSPowerOff->setEnabled(false);
#endif

    // modified fields...

    connect(brightnessSlider, SIGNAL(valueChanged(int)), SLOT(setProfileChanged()));
    connect(disableCompositing, SIGNAL(stateChanged(int)), SLOT(setProfileChanged()));
    connect(idleTime, SIGNAL(valueChanged(int)), SLOT(setProfileChanged()));
    connect(idleCombo, SIGNAL(currentIndexChanged(int)), SLOT(setProfileChanged()));
    connect(freqCombo, SIGNAL(currentIndexChanged(int)), SLOT(setProfileChanged()));
    connect(laptopClosedCombo, SIGNAL(currentIndexChanged(int)), SLOT(setProfileChanged()));

    connect(schemeCombo, SIGNAL(currentIndexChanged(int)), SLOT(setProfileChanged()));
    connect(scriptRequester, SIGNAL(textChanged(const QString&)), SLOT(setProfileChanged()));

#ifdef HAVE_DPMS
    connect(DPMSEnable, SIGNAL(stateChanged(int)), SLOT(enableBoxes()));
    connect(DPMSEnable, SIGNAL(stateChanged(int)), SLOT(setProfileChanged()));
    connect(DPMSSuspend, SIGNAL(valueChanged(int)), SLOT(setProfileChanged()));
    connect(DPMSStandby, SIGNAL(valueChanged(int)), SLOT(setProfileChanged()));
    connect(DPMSPowerOff, SIGNAL(valueChanged(int)), SLOT(setProfileChanged()));
#endif

    connect(profilesList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
            SLOT(switchProfile(QListWidgetItem*, QListWidgetItem*)));

    connect(deleteProfile, SIGNAL(clicked()), SLOT(deleteCurrentProfile()));
    connect(newProfile, SIGNAL(clicked()), SLOT(createProfile()));
    connect(editProfileButton, SIGNAL(clicked()), SLOT(editProfile()));
    connect(importButton, SIGNAL(clicked()), SLOT(importProfiles()));
    connect(exportButton, SIGNAL(clicked()), SLOT(exportProfiles()));
    connect(resetCurrentProfileButton, SIGNAL(clicked()), SLOT(loadProfile()));
    connect(saveCurrentProfileButton, SIGNAL(clicked()), SLOT(saveProfile()));
}

void EditPage::load()
{
    loadProfile();

    enableBoxes();
}

void EditPage::save()
{
}

void EditPage::emitChanged()
{
    emit changed(true);
}

void EditPage::enableBoxes()
{
#ifdef HAVE_DPMS
    DPMSSuspend->setEnabled(DPMSEnable->isChecked());
    DPMSStandby->setEnabled(DPMSEnable->isChecked());
    DPMSPowerOff->setEnabled(DPMSEnable->isChecked());
#endif
}

void EditPage::loadProfile()
{
    kDebug() << "Loading a profile";

    if (!profilesList->currentItem())
        return;

    kDebug() << profilesList->currentItem()->text();

    KConfigGroup *group = new KConfigGroup(m_profilesConfig, profilesList->currentItem()->text());

    if (!group->isValid()) {
        delete group;
        return;
    }
    kDebug() << "Ok, KConfigGroup ready";

    kDebug() << group->readEntry("brightness");

    brightnessSlider->setValue(group->readEntry("brightness").toInt());
    disableCompositing->setChecked(group->readEntry("disableCompositing", false));
    idleTime->setValue(group->readEntry("idleTime").toInt());
    idleCombo->setCurrentIndex(idleCombo->findData(group->readEntry("idleAction").toInt()));
    freqCombo->setCurrentIndex(freqCombo->findData(group->readEntry("cpuPolicy").toInt()));
    schemeCombo->setCurrentIndex(schemeCombo->findText(group->readEntry("scheme")));
    scriptRequester->setPath(group->readEntry("scriptpath"));

    laptopClosedCombo->setCurrentIndex(laptopClosedCombo->findData(group->readEntry("lidAction").toInt()));

#ifdef HAVE_DPMS
    DPMSEnable->setChecked(group->readEntry("DPMSEnabled", false));
    DPMSStandby->setValue(group->readEntry("DPMSStandby", 10));
    DPMSSuspend->setValue(group->readEntry("DPMSSuspend", 30));
    DPMSPowerOff->setValue(group->readEntry("DPMSPowerOff", 60));
#endif

    QVariant var = group->readEntry("disabledCPUs", QVariant());
    QList<QVariant> list = var.toList();

    foreach(const QVariant &ent, list) {
        QCheckBox *box = qobject_cast<QCheckBox*> (CPUListLayout->itemAt(ent.toInt())->widget());

        if (!box)
            continue;

        box->setChecked(true);
    }

    delete group;

    m_profileEdited = false;
    enableSaveProfile();
}

void EditPage::saveProfile(const QString &p)
{
    if (!profilesList->currentItem() && p.isEmpty()) {
        return;
    }

    QString profile;

    if (p.isEmpty()) {
        profile = profilesList->currentItem()->text();
    } else {
        profile = p;
    }

    KConfigGroup *group = new KConfigGroup(m_profilesConfig, profile);

    if (!group->isValid() || !group->entryMap().size()) {
        delete group;
        return;
    }

    group->writeEntry("brightness", brightnessSlider->value());
    group->writeEntry("cpuPolicy", freqCombo->itemData(freqCombo->currentIndex()).toInt());
    group->writeEntry("idleAction", idleCombo->itemData(idleCombo->currentIndex()).toInt());
    group->writeEntry("idleTime", idleTime->value());
    group->writeEntry("lidAction", laptopClosedCombo->itemData(laptopClosedCombo->currentIndex()).toInt());
    group->writeEntry("scheme", schemeCombo->currentText());
    group->writeEntry("scriptpath", scriptRequester->url().path());
    group->writeEntry("disableCompositing", disableCompositing->isChecked());

#ifdef HAVE_DPMS
    group->writeEntry("DPMSEnabled", DPMSEnable->isChecked());
    group->writeEntry("DPMSStandby", DPMSStandby->value());
    group->writeEntry("DPMSSuspend", DPMSSuspend->value());
    group->writeEntry("DPMSPowerOff", DPMSPowerOff->value());
#endif

    QList<int> list;

    for (int i = 0;i < CPUListLayout->count();++i) {
        QCheckBox *box = qobject_cast<QCheckBox*> (CPUListLayout->itemAt(i)->widget());

        if (!box)
            continue;

        if (box->isChecked())
            list.append(i);
    }

    group->writeEntry("disabledCPUs", list);

    group->sync();

    delete group;

    m_profileEdited = false;
    enableSaveProfile();

    emit profilesChanged();
}

void EditPage::reloadAvailableProfiles()
{
    profilesList->clear();

    m_profilesConfig->reparseConfiguration();

    if (m_profilesConfig->groupList().isEmpty()) {
        kDebug() << "No available profiles!";
        return;
    }

    foreach(const QString &ent, m_profilesConfig->groupList()) {
        KConfigGroup *group = new KConfigGroup(m_profilesConfig, ent);
        QListWidgetItem *itm = new QListWidgetItem(KIcon(group->readEntry("iconname")),
                ent);
        profilesList->addItem(itm);
    }

    profilesList->setCurrentRow(0);
}

void EditPage::deleteCurrentProfile()
{
    if (!profilesList->currentItem() || profilesList->currentItem()->text().isEmpty())
        return;

    m_profilesConfig->deleteGroup(profilesList->currentItem()->text());

    m_profilesConfig->sync();

    reloadAvailableProfiles();

    emit profilesChanged();
}

void EditPage::createProfile(const QString &name, const QString &icon)
{
    if (name.isEmpty())
        return;
    KConfigGroup *group = new KConfigGroup(m_profilesConfig, name);

    group->writeEntry("brightness", 80);
    group->writeEntry("cpuPolicy", (int) Solid::Control::PowerManager::Powersave);
    group->writeEntry("idleAction", 0);
    group->writeEntry("idleTime", 50);
    group->writeEntry("lidAction", 0);
    group->writeEntry("turnOffIdle", false);
    group->writeEntry("turnOffIdleTime", 50);
    group->writeEntry("iconname", icon);

    group->sync();

    delete group;

    reloadAvailableProfiles();

    emit profilesChanged();
}

void EditPage::createProfile()
{
    KDialog *dialog = new KDialog(this);
    QWidget *wg = new QWidget();
    KLineEdit *ed = new KLineEdit(wg);
    QLabel *lb = new QLabel(wg);
    QFormLayout *lay = new QFormLayout();
    KIconButton *ibt = new KIconButton(wg);

    ibt->setIconSize(KIconLoader::SizeSmall);

    lb->setText(i18n("Please enter a name for the new profile"));

    lb->setToolTip(i18n("The name for the new profile"));
    lb->setWhatsThis(i18n("Enter here the name for the profile you are creating"));

    ed->setToolTip(i18n("The name for the new profile"));
    ed->setWhatsThis(i18n("Enter here the name for the profile you are creating"));

    lay->addRow(lb);
    lay->addRow(ibt, ed);

    wg->setLayout(lay);

    dialog->setMainWidget(wg);
    ed->setFocus();

    if (dialog->exec() == KDialog::Accepted) {
        createProfile(ed->text(), ibt->icon());
    }
    delete dialog;
}

void EditPage::editProfile(const QString &prevname, const QString &icon)
{
    if (prevname.isEmpty())
        return;

    KConfigGroup *group = new KConfigGroup(m_profilesConfig, prevname);

    group->writeEntry("iconname", icon);

    group->sync();

    delete group;

    reloadAvailableProfiles();

    emit profilesChanged();
}

void EditPage::editProfile()
{
    if (!profilesList->currentItem())
        return;

    KDialog *dialog = new KDialog(this);
    QWidget *wg = new QWidget();
    KLineEdit *ed = new KLineEdit(wg);
    QLabel *lb = new QLabel(wg);
    QFormLayout *lay = new QFormLayout();
    KIconButton *ibt = new KIconButton(wg);

    ibt->setIconSize(KIconLoader::SizeSmall);

    lb->setText(i18n("Please enter a name for this profile"));

    lb->setToolTip(i18n("The name for the new profile"));
    lb->setWhatsThis(i18n("Enter here the name for the profile you are creating"));

    ed->setToolTip(i18n("The name for the new profile"));
    ed->setWhatsThis(i18n("Enter here the name for the profile you are creating"));
    ed->setEnabled(false);

    ed->setText(profilesList->currentItem()->text());

    KConfigGroup *group = new KConfigGroup(m_profilesConfig, profilesList->currentItem()->text());

    ibt->setIcon(group->readEntry("iconname"));

    lay->addRow(lb);
    lay->addRow(ibt, ed);

    wg->setLayout(lay);

    dialog->setMainWidget(wg);
    ed->setFocus();

    if (dialog->exec() == KDialog::Accepted) {
        editProfile(profilesList->currentItem()->text(), ibt->icon());
    }

    delete dialog;
    delete group;
}

void EditPage::importProfiles()
{
    QString fileName = KFileDialog::getOpenFileName(KUrl(), "*.powerdevilprofiles|PowerDevil Profiles "
                       "(*.powerdevilprofiles)", this, i18n("Import PowerDevil profiles"));

    if (fileName.isEmpty()) {
        return;
    }

    KConfig toImport(fileName, KConfig::SimpleConfig);

    foreach(const QString &ent, toImport.groupList()) {
        KConfigGroup copyFrom(&toImport, ent);
        KConfigGroup copyTo(m_profilesConfig, ent);

        copyFrom.copyTo(&copyTo);
    }

    m_profilesConfig->sync();

    reloadAvailableProfiles();

    emit profilesChanged();
}

void EditPage::exportProfiles()
{
    QString fileName = KFileDialog::getSaveFileName(KUrl(), "*.powerdevilprofiles|PowerDevil Profiles "
                       "(*.powerdevilprofiles)", this, i18n("Export PowerDevil profiles"));

    if (fileName.isEmpty()) {
        return;
    }

    kDebug() << "Filename is" << fileName;

    KConfig *toExport = m_profilesConfig->copyTo(fileName);

    toExport->sync();

    delete toExport;
}

void EditPage::switchProfile(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(current)

    if (!m_profileEdited) {
        loadProfile();
    } else {
        int result = KMessageBox::warningYesNoCancel(this, i18n("The current profile has not been saved.\n"
                     "Do you want to save it?"), i18n("Save Profile"));

        if (result == KMessageBox::Yes) {
            saveProfile(previous->text());
            loadProfile();
        } else if (result == KMessageBox::No) {
            loadProfile();
        } else if (result == KMessageBox::Cancel) {
            disconnect(profilesList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
                       this, SLOT(switchProfile(QListWidgetItem*, QListWidgetItem*)));
            profilesList->setCurrentItem(previous);
            connect(profilesList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
                    SLOT(switchProfile(QListWidgetItem*, QListWidgetItem*)));
        }
    }
}

void EditPage::setProfileChanged()
{
    m_profileEdited = true;
    enableSaveProfile();
}

void EditPage::enableSaveProfile()
{
    saveCurrentProfileButton->setEnabled(m_profileEdited);
}

void EditPage::openUrl(const QString &url)
{
    new KRun(KUrl(url), this);
}

#include "EditPage.moc"