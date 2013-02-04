#include "EventDownloadManager.h"
#include "core/BluecherryApp.h"
#include "core/EventData.h"
#include "core/DVRServer.h"
#include "ui/EventVideoDownload.h"
#include "ui/MainWindow.h"
#include "utils/StringUtils.h"
#include <QFileDialog>
#include <QSettings>
#include <QTimer>
#include <QUrl>

#define MAX_CONCURRENT_DOWNLOADS 30

EventDownloadManager::EventDownloadManager(QObject *parent)
    : QObject(parent)
{
     QSettings settings;
     m_lastSaveDirectory = settings.value(QLatin1String("download/lastSaveDirectory")).toString();

     m_checkQueueTimer = new QTimer(this);
     connect(m_checkQueueTimer, SIGNAL(timeout()), this, SLOT(checkQueue()));
     m_checkQueueTimer->start(1000);
}

EventDownloadManager::~EventDownloadManager()
{
}

QString EventDownloadManager::defaultFileName(const EventData &event) const
{
    return withSuffix(event.baseFileName(), QLatin1String(".mkv"));
}

QString EventDownloadManager::absoluteFileName(const QString &fileName) const
{
    QFileInfo fileInfo(fileName);
    if (fileInfo.isAbsolute())
        return fileInfo.absoluteFilePath();

    fileInfo.setFile(QString::fromLatin1("%1/%2").arg(m_lastSaveDirectory).arg(fileName));
    return fileInfo.absoluteFilePath();
}

void EventDownloadManager::updateLastSaveDirectory(const QString &fileName)
{
    QFileInfo fileInfo(fileName);
    QString newSaveDirectory = fileInfo.absoluteDir().absolutePath();

    if (m_lastSaveDirectory != newSaveDirectory)
    {
        m_lastSaveDirectory = newSaveDirectory;

        QSettings settings;
        settings.setValue(QLatin1String("download/lastSaveDirectory"), m_lastSaveDirectory);
    }
}

void EventDownloadManager::startEventDownload(const EventData &event, const QString &fileName)
{
    if (fileName.isEmpty())
        return;

    QString saveFileName = absoluteFileName(withSuffix(fileName, QLatin1String(".mkv")));
    updateLastSaveDirectory(saveFileName);

    QUrl url = event.server->api->serverUrl().resolved(QUrl(QLatin1String("/media/request.php")));
    url.addQueryItem(QLatin1String("id"), QString::number(event.mediaId));

    EventVideoDownload *dl = new EventVideoDownload(url, saveFileName, bcApp->mainWindow);
    m_eventVideoDownloadQueue.append(dl);

    m_eventVideoDownloadList.append(dl);
    connect(dl, SIGNAL(destroyed(QObject*)), this, SLOT(eventVideoDownloadDestroyed(QObject*)));
    emit eventVideoDownloadAdded(dl);
}

void EventDownloadManager::startEventDownload(const EventData &event)
{
    QString fileName = absoluteFileName(defaultFileName(event));
    QString saveFileName = QFileDialog::getSaveFileName(bcApp->mainWindow, tr("Save event video"),
                                                        fileName, tr("Matroska Video (*.mkv)"));

    startEventDownload(event, saveFileName);
}

void EventDownloadManager::startMultipleEventDownloads(const QList<EventData> &events)
{
    QString dirName = QFileDialog::getExistingDirectory(bcApp->mainWindow, tr("Save event videos"),
                                                        m_lastSaveDirectory,
                                                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dirName.isEmpty())
        return;

    foreach (const EventData &event, events)
        startEventDownload(event, QString::fromLatin1("%1/%2").arg(dirName, defaultFileName(event)));
}

void EventDownloadManager::checkQueue()
{
    while (m_activeEventVideoDownloadList.size() < MAX_CONCURRENT_DOWNLOADS && !m_eventVideoDownloadQueue.isEmpty())
    {
        EventVideoDownload *dl = m_eventVideoDownloadQueue.dequeue();
        connect(dl, SIGNAL(finished(EventVideoDownload*)), this, SLOT(eventVideoDownloadFinished(EventVideoDownload*)));
        dl->start();
        m_activeEventVideoDownloadList.append(dl);
    }
}

void EventDownloadManager::eventVideoDownloadFinished(EventVideoDownload *eventVideoDownload)
{
    m_activeEventVideoDownloadList.removeAll(eventVideoDownload);
}

void EventDownloadManager::eventVideoDownloadDestroyed(QObject *destroyedObject)
{
    EventVideoDownload *dl = static_cast<EventVideoDownload *>(destroyedObject);
    m_eventVideoDownloadList.removeAll(dl);
    m_activeEventVideoDownloadList.removeAll(dl);
    m_eventVideoDownloadQueue.removeAll(dl);
    emit eventVideoDownloadRemoved(dl);
}
