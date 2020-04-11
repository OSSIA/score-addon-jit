// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PluginSettingsView.hpp"

#include "PluginSettingsPresenter.hpp"

#include <Library/LibrarySettings.hpp>
#include <PluginSettings/FileDownloader.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <zipdownloader.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(PluginSettings::PluginSettingsView)
namespace PluginSettings
{

namespace zip_helper
{

QString get_path(const QString& str)
{
  auto idx = str.lastIndexOf('/');
  if (idx != -1)
  {
    return str.mid(0, idx);
  }
  return "";
}

QString slash_path(const QString& str)
{
  return {};
}

QString relative_path(const QString& base, const QString& filename)
{
  return filename;
}

QString combine_path(const QString& path, const QString& filename)
{
  return path + "/" + filename;
}

bool make_folder(const QString& str)
{
  QDir d;
  return d.mkpath(str);
}

}

PluginSettingsView::PluginSettingsView()
{
  m_progress->setMinimum(0);
  m_progress->setMaximum(0);
  m_progress->setHidden(true);
  {
    auto local_widget = new QWidget;
    auto local_layout = new QGridLayout{local_widget};
    local_widget->setLayout(local_layout);

    local_layout->addWidget(m_addonsOnSystem);

    m_widget->addTab(local_widget, tr("Local"));
  }

  {
    auto remote_widget = new QWidget;
    auto remote_layout = new QGridLayout{remote_widget};
    remote_layout->addWidget(m_remoteAddons, 0, 0, 2, 1);

    auto vlay = new QVBoxLayout;
    vlay->addWidget(m_refresh);
    vlay->addWidget(m_install);
    vlay->addWidget(m_progress);
    vlay->addStretch();
    remote_layout->addLayout(vlay, 0, 1, 1, 1);

    m_widget->addTab(remote_widget, tr("Browse"));
  }

  for (QTableView* v : {m_addonsOnSystem, m_remoteAddons})
  {
    v->horizontalHeader()->hide();
    v->verticalHeader()->hide();
    v->verticalHeader()->sectionResizeMode(QHeaderView::Fixed);
    v->verticalHeader()->setDefaultSectionSize(40);
    v->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    v->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    v->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->setEditTriggers(QAbstractItemView::NoEditTriggers);
    v->setSelectionMode(QAbstractItemView::SingleSelection);
    v->setShowGrid(false);
  }

  connect(
      &mgr,
      &QNetworkAccessManager::finished,
      this,
      &PluginSettingsView::on_message);

  connect(m_refresh, &QPushButton::pressed, this, [this]() {
    RemotePluginItemModel* model
        = static_cast<RemotePluginItemModel*>(m_remoteAddons->model());
    model->clear();

    m_progress->setVisible(true);

    QNetworkRequest rqst{
        QUrl("https://raw.githubusercontent.com/OSSIA/score-addons/master/"
             "addons.json")};
    mgr.get(rqst);
  });

  connect(
      m_install, &QPushButton::pressed, this, &PluginSettingsView::install);
}

QWidget* PluginSettingsView::getWidget()
{
  return m_widget;
}

void PluginSettingsView::handleAddonList(const QJsonObject& obj)
{
  m_progress->setVisible(true);
  auto arr = obj["addons"].toArray();
  m_addonsToRetrieve = arr.size();
  for (QJsonValue elt : arr)
  {
    QNetworkRequest rqst{QUrl(elt.toString())};
    mgr.get(rqst);
  }
}

void PluginSettingsView::handleAddon(const QJsonObject& obj)
{
  m_addonsToRetrieve--;
  if (m_addonsToRetrieve == 0)
  {
    m_progress->setHidden(true);
  }

  auto addon = RemoteAddon::fromJson(obj);
  if (!addon)
    return;

  auto& add = *addon;

  // Load images
  RemotePluginItemModel* model
      = static_cast<RemotePluginItemModel*>(m_remoteAddons->model());
  if (!add.smallImagePath.isEmpty())
  {
    // c.f. https://wiki.qt.io/Download_Data_from_URL
    auto dl = new score::FileDownloader{QUrl{add.smallImagePath}};
    connect(dl, &score::FileDownloader::downloaded, this, [=](QByteArray arr) {
      model->updateAddon(add.key, [=](RemoteAddon& add) {
        add.smallImage.loadFromData(arr);
      });

      dl->deleteLater();
    });
  }

  if (!add.largeImagePath.isEmpty())
  {
    // c.f. https://wiki.qt.io/Download_Data_from_URL
    auto dl = new score::FileDownloader{QUrl{add.largeImagePath}};
    connect(dl, &score::FileDownloader::downloaded, this, [=](QByteArray arr) {
      model->updateAddon(add.key, [=](RemoteAddon& add) {
        add.largeImage.loadFromData(arr);
      });

      dl->deleteLater();
    });
  }

  model->addAddon(std::move(add));
}

void PluginSettingsView::on_message(QNetworkReply* rep)
{
  auto res = rep->readAll();
  auto json = QJsonDocument::fromJson(res).object();

  if (json.contains("addons"))
  {
    handleAddonList(json);
  }
  else if (json.contains("name"))
  {
    handleAddon(json);
  }
  else
  {
    qDebug() << res;
    m_progress->setHidden(true);
  }

  rep->deleteLater();
}

void PluginSettingsView::install()
{
  auto& remotePlugins
      = *static_cast<RemotePluginItemModel*>(m_remoteAddons->model());

  auto rows = m_remoteAddons->selectionModel()->selectedRows(0);
  if (rows.empty())
    return;

  auto num = rows.first().row();
  SCORE_ASSERT(remotePlugins.addons().size() > num);
  auto& addon = remotePlugins.addons().at(num);

  if (addon.source == QUrl{})
    return;

  m_progress->setVisible(true);

  const QString addons_path{score::AppContext()
                               .settings<Library::Settings::Model>()
                               .getPath()
                           + "/Addons"};
  zdl::download_and_extract(
        addon.source,
        QFileInfo{addons_path}.absolutePath(),
        [=] (const std::vector<QString>& res) {
    m_progress->setHidden(true);
    if(res.empty())
      return;
    // We want the extracted folder to have the name of the addon
    {
      QDir addons_dir{addons_path};
      QFileInfo a_file(res[0]);
      auto d = a_file.dir();
      auto old_d = d;
      while (d.cdUp() && !d.isRoot())
      {
        if (d == addons_dir)
        {
          addons_dir.rename(old_d.dirName(), addon.raw_name);
          break;
        }
        old_d = d;
      }
    }


    QMessageBox::information(
        m_widget,
        tr("Addon downloaded"),
        tr("The addon %1 has been succesfully installed in :\n"
           "%2\n\n"
           "It will be built and enabled shortly.\nCheck the message "
           "console for errors if nothing happens.")
            .arg(addon.name)
            .arg(QFileInfo(addons_path).absoluteFilePath()));

  },
  [this] {
    m_progress->setHidden(true);
    QMessageBox::warning(
        m_widget,
        tr("Download failed"),
        tr("The addon %1 could not be downloaded."));
  });
}

}
