#include <JitCpp/ApplicationPlugin.hpp>
#include <JitCpp/MetadataGenerator.hpp>

#include <Library/LibrarySettings.hpp>

#include <core/application/ApplicationInterface.hpp>
#include <core/view/Window.hpp>

#include <QApplication>
#include <QDir>
#include <QThread>
#include <QDirIterator>
#include <QQuickWidget>
namespace Jit
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
  : score::GUIApplicationPlugin{ctx}
{
  con(m_addonsWatch, &QFileSystemWatcher::directoryChanged,
      this, [&] (const QString& a) {
    QTimer::singleShot(5000, [=]
    {
      rescanAddons();
    });
  });
  con(m_addonsWatch, &QFileSystemWatcher::fileChanged,
      this, &ApplicationPlugin::updateAddon);

  con(m_nodesWatch, &QFileSystemWatcher::fileChanged,
      this, &ApplicationPlugin::setupNode);

  con(m_compiler, &AddonCompiler::jobCompleted,
      this, &ApplicationPlugin::registerAddon, Qt::QueuedConnection);
}

void ApplicationPlugin::rescanAddons()
{
  const auto& libpath = context.settings<Library::Settings::Model>().getPath();
  auto addons = libpath + "/Addons";
  m_addonsWatch.addPath(addons);
  QDirIterator it{addons, QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot, QDirIterator::NoIteratorFlags};
  while(it.hasNext())
  {
    it.next();
    auto p = it.fileInfo().filePath();
    if(!m_addonsPaths.contains(p))
    {
      m_addonsPaths.insert(p);
      setupAddon(p);
    }
  }
}
void ApplicationPlugin::rescanNodes()
{
  const auto& libpath = context.settings<Library::Settings::Model>().getPath();
  auto nodes = libpath + "/Nodes";
  m_nodesWatch.addPath(nodes);

  QDirIterator it{nodes, QDir::Filter::Files | QDir::Filter::NoDotAndDotDot, QDirIterator::Subdirectories};
  while(it.hasNext())
  {
    auto path = it.next();
    m_nodesWatch.addPath(path);
    if(!m_nodesPaths.contains(path))
    {
      m_nodesPaths.insert(path);
      setupAddon(path);
    }
    setupNode(path);
  }

}
void ApplicationPlugin::initialize()
{
  return;
  rescanNodes();
  rescanAddons();
  delete new QQuickWidget;
}

void ApplicationPlugin::registerAddon(score::Plugin_QtInterface* p)
{
  qDebug() << "registerAddon => " << typeid(p).name();
  score::GUIApplicationInterface::instance().registerPlugin(*p);
  qDebug() << "JIT addon registered" << p;
}

void ApplicationPlugin::setupAddon(const QString& addon)
{
  qDebug() << "Registering JIT addon" << addon;
  QFileInfo addonInfo{addon};
  auto addonFolderName = addonInfo.fileName();
  if(addonFolderName == "Nodes")
    return;

  auto [json, cpp_files, files] = loadAddon(addon);

  if(cpp_files.empty())
    return;

  auto addon_files_path = generateAddonFiles(addonFolderName, addon, files);
  std::vector<std::string> flags = {
    "-I" + addon.toStdString()
  , "-I" + addon_files_path.toStdString()};

  const std::string id = json["key"].toString().remove(QChar('-')).toStdString();
  m_compiler.submitJob(id, cpp_files, flags);
}

void ApplicationPlugin::setupNode(const QString& f)
{
  QFileInfo fi{f};
  if(fi.suffix() == "hpp" || fi.suffix() == "cpp")
  {
    if(QFile file{f}; file.open(QIODevice::ReadOnly))
    {
      auto node = file.readAll();
      constexpr auto make_uuid_s = "make_uuid";
      auto make_uuid = node.indexOf(make_uuid_s);
      if(make_uuid == -1)
        return;
      int umin = node.indexOf('"', make_uuid + 9);
      if(umin == -1)
        return;
      int umax = node.indexOf('"', umin + 1);
      if(umax == -1)
        return;
      if((umax - umin) != 37)
        return;
      auto uuid = QString{node.mid(umin + 1, 36)};
      uuid.remove(QChar('-'));

      node.append(
            R"_(
            #include <score/plugins/PluginInstances.hpp>

            SCORE_EXPORT_PLUGIN(Control::score_generic_plugin<Node>)
            )_");

      qDebug() << "Registering JIT node" << f;
      m_compiler.submitJob(uuid.toStdString(), node.toStdString(), {});
    }
  }
}

void ApplicationPlugin::updateAddon(const QString& f)
{
  qDebug() << f;
}


}
