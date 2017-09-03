#include "searchwidget.h"
#include "searchresultsmodel.h"
#include "searcher.h"
#include "shellcommandmodel.h"
#include "shortcutmanager.h"
#include "fileviewer.h"

#include <QLabel>
#include <QLineEdit>
#include <QTreeView>
#include <QCheckBox>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QDir>
#include <QPushButton>
#include <QSettings>
#include <QAction>

namespace
{
const QString qs_geometry = "search/geometry";
const QString qs_files = "search/files";
const QString qs_text = "search/text";
const QString qs_recursive = "search/recursive";
const QString qs_caseSensitive = "search/caseSensitive";
}

SearchWidget::SearchWidget (ShellCommandModel *commanRunner, QWidget *parent) :
  QWidget (parent),
  commandRunner_ (commanRunner),
  dir_ (new QLineEdit (this)),
  filePattern_ (new QLineEdit (this)),
  text_ (new QLineEdit (this)),
  recursive_ (new QCheckBox (tr ("Recursive"), this)),
  caseSensitive_ (new QCheckBox (tr ("Case sensitive"), this)),
  buttons_ (new QDialogButtonBox (QDialogButtonBox::Apply |
                                  QDialogButtonBox::Abort, this)),
  results_ (new QTreeView (this)),
  model_ (new SearchResultsModel (this)),
  searcher_ (new Searcher (this))
{
  setRunning (false);
  connect (buttons_->button (QDialogButtonBox::Apply), &QPushButton::clicked,
           this, &SearchWidget::start);
  connect (buttons_->button (QDialogButtonBox::Abort), &QPushButton::clicked,
           this, &SearchWidget::abort);

  dir_->setText (QDir::homePath ());

  filePattern_->setText (QLatin1String ("*"));

  results_->setModel (model_);
  results_->hideColumn (SearchResultsModel::ByteOffset);

  connect (searcher_, &Searcher::foundText,
           model_, &SearchResultsModel::addText);
  connect (searcher_, &Searcher::foundFile,
           model_, &SearchResultsModel::addFile);
  connect (searcher_, &Searcher::finished,
           this, &SearchWidget::finished);

  auto view = ShortcutManager::create (this, ShortcutManager::View);
  connect (view, &QAction::triggered,
           this, &SearchWidget::viewCurrent);

  auto edit = ShortcutManager::create (this, ShortcutManager::OpenInEditor);
  connect (edit, &QAction::triggered,
           this, &SearchWidget::editCurrent);


  {
    auto layout = new QGridLayout (this);
    auto row = 0;
    layout->addWidget (new QLabel (tr ("Search in:")), row, 0);
    layout->addWidget (dir_, row, 1);

    ++row;
    layout->addWidget (new QLabel (tr ("File pattern:")), row, 0);
    layout->addWidget (filePattern_, row, 1);

    ++row;
    layout->addWidget (new QLabel (tr ("Search text:")), row, 0);
    layout->addWidget (text_, row, 1);

    ++row;
    layout->addWidget (recursive_, row, 0);
    layout->addWidget (caseSensitive_, row, 1);

    ++row;
    layout->addWidget (buttons_, row, 0, 1, 2);

    ++row;
    layout->addWidget (new QLabel (tr ("Results")), row, 0, 1, 2);

    ++row;
    layout->addWidget (results_, row, 0, 1, 2);
  }


  QSettings settings;
  restoreState (settings);
}

SearchWidget::~SearchWidget ()
{
  QSettings settings;
  saveState (settings);
}

void SearchWidget::saveState (QSettings &settings) const
{
  settings.setValue (qs_geometry, saveGeometry ());
  settings.setValue (qs_files, filePattern_->text ());
  settings.setValue (qs_text, text_->text ());
  settings.setValue (qs_recursive, recursive_->isChecked ());
  settings.setValue (qs_caseSensitive, caseSensitive_->isChecked ());
}

void SearchWidget::restoreState (QSettings &settings)
{
  restoreGeometry (settings.value (qs_geometry).toByteArray ());
  filePattern_->setText (settings.value (qs_files, QLatin1String ("*")).toString ());
  text_->setText (settings.value (qs_text).toString ());
  recursive_->setChecked (settings.value (qs_recursive, true).toBool ());
  caseSensitive_->setChecked (settings.value (qs_caseSensitive, false).toBool ());
}

void SearchWidget::setRunning (bool isRunning)
{
  buttons_->button (QDialogButtonBox::Apply)->setEnabled (!isRunning);
  buttons_->button (QDialogButtonBox::Abort)->setEnabled (isRunning);
}

void SearchWidget::setDefaultDir (const QString &path)
{
  dir_->setText (path);
}

void SearchWidget::start ()
{
  const auto dirs = dir_->text ().split (',');
  if (dirs.isEmpty ())
  {
    return;
  }

  model_->clear ();

  searcher_->setRecursive (recursive_->isChecked ());
  searcher_->setFilePatterns (filePattern_->text ().split (','));
  const auto caseSence = caseSensitive_->isChecked () ? Qt::CaseSensitive
                                                      : Qt::CaseInsensitive;
  searcher_->setText (text_->text (), caseSence);

  searcher_->startAsync (dirs);

  setRunning (true);
}

void SearchWidget::abort ()
{
  searcher_->abort ();
}

void SearchWidget::finished ()
{
  setRunning (false);
}

void SearchWidget::viewCurrent ()
{
  auto file = model_->fileName (results_->currentIndex ());
  if (!file.isEmpty ())
  {
    auto view = new FileViewer;
    view->showFile (file);
  }
}

void SearchWidget::editCurrent ()
{
  auto file = model_->fileName (results_->currentIndex ());
  if (!file.isEmpty ())
  {
    commandRunner_->openInEditor (file);
  }
}
