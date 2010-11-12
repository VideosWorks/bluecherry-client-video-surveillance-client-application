#include "ImageDecodeTask.h"
#include <QImageReader>
#include <QBuffer>
#include <QDebug>

ImageDecodeTask::ImageDecodeTask(QObject *caller, const char *callback)
    : ThreadTask(caller, callback)
{
}

void ImageDecodeTask::runTask()
{
    if (isCancelled() || m_data.isNull())
        return;

    QBuffer buffer(&m_data);
    if (!buffer.open(QIODevice::ReadOnly))
    {
        qDebug() << "Image decoding error:" << buffer.errorString();
        return;
    }

    QImageReader reader(&buffer, "jpeg");
    /* This would be more efficient, but causes the decoding to fail with
     * Qt 4.6.2 on Ubuntu 10.04. Disabled for now as a result. Issue #473 */
    //reader.setAutoDetectImageFormat(false);

    if (!reader.read(&m_result))
    {
        qDebug() << "Image decoding error:" << reader.errorString();
        return;
    }
    buffer.close();
    m_data.clear();

    m_scaleResults.resize(m_scaleSizes.size());
    for (int i = 0; i < m_scaleSizes.size(); ++i)
    {
        m_scaleResults[i] = m_result.scaled(m_scaleSizes[i], Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
}
