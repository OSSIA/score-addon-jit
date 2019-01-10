#pragma once
#include <JitCpp/AddonCompiler.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <QFileSystemWatcher>
#include <QSet>
#include <QThread>

namespace Jit
{

struct ApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
  ApplicationPlugin(const score::GUIApplicationContext& ctx);

  void setupAddon(const QString& addon);
  void registerAddon(jit_plugin*);
  void updateAddon(const QString& addon);

  void setupNode(const QString& addon);
  void initialize() override;

  void rescanAddons();
  void rescanNodes();

  QFileSystemWatcher m_addonsWatch;
  QFileSystemWatcher m_nodesWatch;
  QSet<QString> m_addonsPaths;
  QSet<QString> m_nodesPaths;
  AddonCompiler m_compiler;
};
}
