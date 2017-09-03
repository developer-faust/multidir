#include "searcher.h"
#include "debug.h"
#include "utils.h"

#include <QtConcurrentRun>
#include <QDir>
#include <QFile>
#include <QRegExp>
#include <QByteArrayMatcher>
#include <QTextCodec>

Searcher::Searcher (QObject *parent) :
  QObject (parent),
  isAborted_ (false),
  options_ ()
{

}

Searcher::~Searcher ()
{
}

void Searcher::setRecursive (bool isOn)
{
  options_.recursive = isOn;
}

void Searcher::setFilePatterns (const QStringList &filePatterns)
{
  options_.filePatterns.clear ();
  options_.filePatterns.reserve (filePatterns.size ());
  for (const auto &pattern: filePatterns)
  {
    QRegExp re (pattern);
    re.setPatternSyntax (QRegExp::Wildcard);
    re.setCaseSensitivity (Qt::CaseInsensitive);
    options_.filePatterns.append (re);
  }
}

void Searcher::setText (const QString &text)
{
  auto *codec = QTextCodec::codecForLocale ();
  options_.text.setPattern (codec->fromUnicode (text));
  const auto textLength = options_.text.pattern ().size ();
  options_.maxOccurenceLength = options_.sideContextLength * 2 + textLength;
  options_.textDecoder = codec->makeDecoder (QTextCodec::ConvertInvalidToNull);
}

void Searcher::startAsync (const QStringList &dirs)
{
  isAborted_ = false;

  QtConcurrent::run (this, &Searcher::searchFiles, dirs, options_, 0);
}

void Searcher::abort ()
{
  isAborted_ = true;
}

void Searcher::searchFiles (QStringList dirs, Options options, int depth)
{
  for (const auto &dir: dirs)
  {
    if (isAborted_)
    {
      break;
    }

    QDir d (dir);

    if (options.recursive)
    {
      QStringList subdirs;
      for (const auto &subdir: d.entryList (QDir::Dirs | QDir::NoDotAndDotDot))
      {
        subdirs.append (d.absoluteFilePath (subdir));
      }
      if (!subdirs.isEmpty ())
      {
        searchFiles (subdirs, options, depth + 1);
      }
    }

    for (const auto &fileName: d.entryList (QDir::Files | QDir::Hidden | QDir::System))
    {
      if (isAborted_)
      {
        break;
      }

      auto passPattern = options.filePatterns.isEmpty ();
      for (const auto &filter: options.filePatterns)
      {
        passPattern |= filter.exactMatch (fileName);
        if (passPattern)
        {
          break;
        }
      }

      if (!passPattern)
      {
        continue;
      }

      const auto fullName = d.absoluteFilePath (fileName);
      if (!options.text.pattern ().isEmpty ())
      {
        searchText (fullName, options);
      }
      else
      {
        emit foundFile (fullName);
      }
    }
  }

  if (!depth)
  {
    emit finished ();
  }
}

void Searcher::searchText (const QString &fileName, Searcher::Options options)
{
  QFile f (fileName);
  if (!f.open (QFile::ReadOnly))
  {
    return;
  }

  auto offset = 0;
  while (!f.atEnd ())
  {
    const auto line = f.readLine ();
    auto start = 0;

    while (true)
    {
      auto index = options.text.indexIn (line, start);
      if (index == -1)
      {
        break;
      }
      auto context = line;
      if (context.length () > options.maxOccurenceLength)
      {
        const auto start = std::max (0, index - options.sideContextLength);
        context = context.mid (start, options.maxOccurenceLength);
      }
      const auto text = options.textDecoder->toUnicode (context).trimmed ();
      emit foundText (fileName, offset + index, text);
      start = index + 1;
    }

    offset += line.size ();
  }
}

#include "moc_searcher.cpp"
