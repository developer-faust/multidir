#include "viewer.h"
#include "debug.h"
#include "notifier.h"

#include <QTextEdit>
#include <QBoxLayout>
#include <QMessageBox>
#include <QAction>
#include <QFile>
#include <QtConcurrentRun>
#include <QFileInfo>
#include <QTextCodec>
#include <QSettings>

namespace
{
const QString qs_grometry = "viewer/geometry";
}

Viewer::Viewer (QWidget *parent) :
  QWidget (parent),
  edit_ (new QTextEdit (this)),
  watcher_ (nullptr)
{
  setWindowIcon (QIcon (":/app.png"));
  setAttribute (Qt::WA_DeleteOnClose);
  setWindowModality (Qt::WindowModality::ApplicationModal);

  auto *close = new QAction (this);
  close->setShortcut ({Qt::Key_Escape});
  close->setShortcutContext (Qt::WidgetWithChildrenShortcut);
  addAction (close);
  connect (close, &QAction::triggered,
           this, &QWidget::close);

  edit_->setReadOnly (true);

  auto layout = new QVBoxLayout (this);
  layout->addWidget (edit_);


  QSettings settings;
  restoreGeometry (settings.value (qs_grometry, saveGeometry ()).toByteArray ());
}

Viewer::~Viewer ()
{
  QSettings settings;
  settings.setValue (qs_grometry, saveGeometry ());
}

bool Viewer::setFile (const QString &name)
{
  const QFileInfo info (name);
  setWindowTitle (info.absoluteFilePath ());

  if (!info.exists () || !info.isReadable ())
  {
    Notifier::error (tr ("Failed to view file '%1'").arg (info.absoluteFilePath ()));
    return false;
  }

  const auto size = info.size ();
  const auto bigSize = 1 * 1024 * 1024;
  if (size > bigSize)
  {
    const auto res = QMessageBox::question (this, {}, tr ("File is too big. Continue?"));
    if (res == QMessageBox::No)
    {
      return false;
    }
  }

  readInBackground (info.absoluteFilePath ());
  return true;
}

void Viewer::showFile (const QString &name)
{
  show ();
  if (!setFile (name))
  {
    close ();
  }
}

void Viewer::readInBackground (const QString &name)
{
  ASSERT (!watcher_);
  watcher_ = new QFutureWatcher<QString>(this);
  connect (watcher_, &QFutureWatcher<QString>::finished,
           this, &Viewer::setFileContents);

  watcher_->setFuture (
    QtConcurrent::run (
      [](QString name) -> QString {
        QFile f (name);
        if (f.open (QFile::ReadOnly))
        {
          auto codec = QTextCodec::codecForName ("UTF-8");
          auto decoder = codec->makeDecoder (QTextCodec::ConvertInvalidToNull);
          auto data = decoder->toUnicode (f.readAll ());
          for (auto i = 0, end = data.size (); i < end; ++i)
          {
            if (data[i].isNull ())
            {
              data[i] = QLatin1Char ('.');
            }
          }
          return data;
        }
        Notifier::error (tr ("Failed to read file '%1': '%2'").arg (name, f.errorString ()));
        return {};
      }, name));
}

void Viewer::setFileContents ()
{
  ASSERT (watcher_);
  edit_->setText (watcher_->result ());
  watcher_->deleteLater ();
  watcher_ = nullptr;
}
