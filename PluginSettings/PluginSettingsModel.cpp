// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PluginSettingsModel.hpp"

#include <score/application/ApplicationContext.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QApplication>
#include <QDebug>
#include <QSet>
#include <QSettings>
#include <QStandardItemModel>
#include <QStringList>
#include <QVariant>
#include <qnamespace.h>
namespace PluginSettings
{
PluginSettingsModel::PluginSettingsModel(
    QSettings& set,
    const score::ApplicationContext& ctx)
    : score::SettingsDelegateModel{}
    , localPlugins{ctx}
    , remoteSelection{&remotePlugins}
{
}

PluginSettingsModel::~PluginSettingsModel() {}
}
