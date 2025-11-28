#include "PipelineBase.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include "Main/global.h"

PipelineBase::PipelineBase(QObject *parent)
    : QObject(parent)
{
}

void PipelineBase::setInput(const QStringList &paths, const QString &pipelineName)
{
    m_sourcePaths  = paths;
    m_pipelineName = pipelineName;

    if (m_sourcePaths.isEmpty()) {
        qWarning() << "PipelineBase::setInput: empty source path list";
        return;
    }

    QFileInfo lastInfo(m_sourcePaths.last());
    m_srcFolder = lastInfo.absolutePath();
    m_srcBase   = lastInfo.completeBaseName();
    m_srcExt    = lastInfo.suffix();   // no dot

    //
    // Project root:   <srcBase>_<pipelineName>
    //
    QString projName = QString("%1_%2").arg(m_srcBase, m_pipelineName);
    m_projFolderPath = QDir(m_srcFolder).absoluteFilePath(projName);

    //
    // Stage directories
    //
    m_alignDir  = QDir(m_projFolderPath).absoluteFilePath("align");
    m_grayDir   = QDir(m_projFolderPath).absoluteFilePath("gray");
    m_focusDir  = QDir(m_projFolderPath).absoluteFilePath("focus");
    m_depthDir  = QDir(m_projFolderPath).absoluteFilePath("depth");
    m_fusionDir = QDir(m_projFolderPath).absoluteFilePath("fusion");

    //
    // Reset stage outputs
    //
    m_alignedPaths.clear();
    m_alignedGrayPaths.clear();
    m_focusMapPaths.clear();
    m_depthRawPath.clear();
    m_depthFilteredPath.clear();
    m_fusionOutputPath.clear();
}

void PipelineBase::prepareProjectStructure()
{
    if (m_projFolderPath.isEmpty()) return;

    QDir dir;
    dir.mkpath(m_projFolderPath);
    dir.mkpath(m_alignDir);
    dir.mkpath(m_grayDir);
    dir.mkpath(m_focusDir);
    dir.mkpath(m_depthDir);
    dir.mkpath(m_fusionDir);
}

void PipelineBase::prepareAlignmentPaths()
{
    m_alignedPaths.clear();
    m_alignedGrayPaths.clear();

    if (m_sourcePaths.isEmpty()) return;

    for (const QString &src : m_sourcePaths) {
        QFileInfo fi(src);
        QString base = fi.completeBaseName();

        QString aligned = QDir(m_alignDir).absoluteFilePath(
            QString("aligned_%1.%2").arg(base, m_srcExt));

        QString gray = QDir(m_grayDir).absoluteFilePath(
            QString("gray_%1.%2").arg(base, m_srcExt));

        m_alignedPaths     << aligned;
        m_alignedGrayPaths << gray;
    }
}

void PipelineBase::prepareFocusMapPaths()
{
    m_focusMapPaths.clear();

    if (m_sourcePaths.isEmpty()) return;

    for (const QString &src : m_sourcePaths) {
        QFileInfo fi(src);
        QString base = fi.completeBaseName();

        QString desiredBase = QString("focus_%1").arg(base);
        QString out = uniqueBaseName(m_focusDir, desiredBase, ".png");
        m_focusMapPaths << out;
    }
}

void PipelineBase::prepareDepthPaths()
{
    //
    // Depth raw + depth filtered
    //
    QString rawBase   = QString("%1_depthmap").arg(m_srcBase);
    QString filtBase  = QString("%1_depthmap_filtered").arg(m_srcBase);

    m_depthRawPath      = uniqueBaseName(m_depthDir, rawBase, ".png");
    m_depthFilteredPath = uniqueBaseName(m_depthDir, filtBase, ".png");
}

void PipelineBase::prepareFusionPath()
{
    QString fusionBase = QString("%1_pmax_FocusStack").arg(m_srcBase);
    QString extDot     = QString(".%1").arg(m_srcExt);
    m_fusionOutputPath = uniqueBaseName(m_fusionDir, fusionBase, extDot);
}

QString PipelineBase::uniqueBaseName(const QString &folder,
                                     const QString &base,
                                     const QString &ext)
{
    QDir dir(folder);
    QString candidate = dir.absoluteFilePath(base + ext);

    if (!QFileInfo::exists(candidate))
        return candidate;

    int idx = 1;
    while (true) {
        QString numbered = dir.absoluteFilePath(
            QString("%1_%2%3").arg(base).arg(idx).arg(ext));
        if (!QFileInfo::exists(numbered))
            return numbered;
        ++idx;
    }
}

void PipelineBase::clean()
{
    QFile(m_projFolderPath).moveToTrash();
}

void PipelineBase::incrementProgress() {
    emit progress(++m_progressCount, m_total);
    // qDebug() << "PipelineBase::incrementProgress"
    //          << m_progressCount << m_total;
    msg = QString::number(m_progressCount) + " / " + QString::number(m_total);
    G::log("PipelineBase::incrementProgress", msg);
}
