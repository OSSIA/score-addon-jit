#pragma once
#include <PluginSettings/PluginItemModel.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <QItemSelectionModel>

class QAbstractItemModel;

namespace PluginSettings
{
class BlacklistCommand;

class PluginSettingsModel : public score::SettingsDelegateModel
{
public:
  PluginSettingsModel(QSettings& set, const score::ApplicationContext& ctx);
  ~PluginSettingsModel();

  LocalPluginItemModel localPlugins;
  RemotePluginItemModel remotePlugins;
  QItemSelectionModel remoteSelection;
};
}
