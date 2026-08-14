///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024-2026 Wang Yao <wangyao1052@163.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////

#include "FileCommands.h"

#include <QObject>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wyapDocument.h>
#include <wy3dDatumPlane.h>
#include <wy3dSheet.h>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <wy3dSketch.h>
#include <wy3dDatabase.h>

#include "application/Application.h"
#include "application/AutoSave.h"
#include "widgets/frame/MainWindow.h"
#include "test/Test.h"
#include "exporter/Exporter.h"
#include "exporter/SketchExporter.h"
#include "exporter/Importer.h"
#include "scene/Scene.h"
#include "utils/MessageBoxUtil.h"
#include "widgets/panels/output/OutputWidget.h"
#include "application/PythonScriptExecutor.h"
#include "application/Document.h"
#include "view/ViewUtil.h"
#include "view/OsgView.h"

#include "commands/CommandNames.h"
#include "widgets/panels/DockPanelIds.h"
#include "widgets/panels/DockPanelManager.h"


FileCmdsUtil::UiAskRet FileCmdsUtil::uiAskWhetherSaveDocument(wyap::Document* pDoc)
{
    if (!pDoc)
    {
        assert(false);
        return UiAskRet::Continue;
    }

    if (!pDoc->hasStatus(wyap::DocumentStatus::Modified))
    {
        return UiAskRet::Continue;
    }

    const std::string& fileName = pDoc->getFileName();
    QString text = QCoreApplication::translate("FileCmds", "Do you want to save the changes to") + "\n" + QString::fromStdString(fileName) + "?";
    int ret = QMessageBox::question(Application::instance().getMainWindow(), "Yi3D", text,
        QCoreApplication::translate("FileCmds", "Yes"),
        QCoreApplication::translate("FileCmds", "No"),
        QCoreApplication::translate("FileCmds", "Cancel"),
        0);
    if (0 == ret) // Yes
    {
        bool isUserCanceled(false);
        FileCmdsUtil::uiSaveFile(pDoc, false, isUserCanceled);
        if (isUserCanceled)
            return UiAskRet::Cancel;
        else
            return UiAskRet::Continue;
    }
    else if (1 == ret) // No
    {
        return UiAskRet::Continue;
    }
    else if (2 == ret) // Cancel
    {
        return UiAskRet::Cancel;
    }
    else
    {
        assert(false);
        return UiAskRet::Cancel;
    }

    return UiAskRet::Continue;
}

bool FileCmdsUtil::uiSaveFile(wyap::Document* pDoc, bool isSaveAs, bool& isUserCanceled)
{
    assert(pDoc);
    isUserCanceled = false;

    std::string u8FileName = pDoc->getFileName();
    // The file name's suffix decides the format; the dialog block below may
    // refine it when the user picks a new name or filter.
    wydb::FileType fileType = FileCmdsUtil::inferFileType(u8FileName);
    if (isSaveAs || pDoc->hasStatus(wyap::DocumentStatus::NewlyCreated))
    {
        // 构建两种格式的filter
        const QString textExt = QString::fromStdString(wy3d::Database::extension(wydb::FileType::Text));
        const QString binaryExt = QString::fromStdString(wy3d::Database::extension(wydb::FileType::Binary));
        const QString textFilter = QCoreApplication::translate(
            "FileCmds",
            "YI3D Text files (*.%1)").arg(textExt);
        const QString binaryFilter = QCoreApplication::translate(
            "FileCmds",
            "YI3D Binary files (*.%1)").arg(binaryExt);
        const QString filter = textFilter + ";;" + binaryFilter;

        QString selectedFilter;
        // Pre-select the filter that matches the current file name's suffix,
        // so the default choice cannot silently switch the format.
        {
            const QString suffix = QFileInfo(QString::fromStdString(u8FileName)).suffix().toLower();
            if (suffix == QString::fromStdString(wy3d::Database::extension(wydb::FileType::Binary)))
            {
                selectedFilter = binaryFilter;
            }
            else
            {
                selectedFilter = textFilter;
            }
        }
        QString fileFullPath = QFileDialog::getSaveFileName(
            Application::instance().getMainWindow(),
            QCoreApplication::translate("FileCmds", "Save file"),
            QString::fromStdString(u8FileName),
            filter,
            &selectedFilter,
            QFileDialog::DontUseNativeDialog);
        if (fileFullPath.isEmpty() || fileFullPath.isNull())
        {
            isUserCanceled = true;
            return false;
        }

        // 根据用户选择的filter决定文件类型
        if (selectedFilter == binaryFilter)
        {
            fileType = wydb::FileType::Binary;
        }

        // Ensure the saved file name uses the expected extension when the user
        // does not type one explicitly.
        QFileInfo fileInfo(fileFullPath);
        if (fileInfo.suffix().isEmpty())
        {
            fileFullPath += "." + QString::fromStdString(wy3d::Database::extension(fileType));
        }

        // The final file name's suffix is authoritative for the format: a file
        // named *.wy3db must never contain text-format data, even if the
        // dialog's filter was changed to Text.
        {
            QFileInfo finalInfo(fileFullPath);
            const QString suffix = finalInfo.suffix().toLower();
            if (suffix == QString::fromStdString(wy3d::Database::extension(wydb::FileType::Binary)))
            {
                fileType = wydb::FileType::Binary;
            }
            else
            {
                fileType = wydb::FileType::Text;
            }
        }

        u8FileName = fileFullPath.toStdString();
    }

    wy::ErrorStatus error = Application::instance().getDocManager()->saveDocument(pDoc, u8FileName, {fileType});
    if (wy::ErrorStatus::Ok == error)
    {
        // Manual save succeeded: delete the document's .autosave and refresh
        // the autosave time (must be after saveDocument, when getFileName()
        // already returns the saved path).
        Application::instance().getAutoSave()->onDocumentSaved(pDoc);

        Application::instance().getMainWindow()->setWindowTitle(QString::fromStdString(u8FileName));
        return true;
    }
    else
    {
        // TODO 弹出错误提示窗口
        assert(false);
        return false;
    }
}

void FileCmdsUtil::fitViewToAll(wyap::Document* pDoc)
{
    assert(pDoc);

    wyap::View* pView = Application::instance().getViewManager()->getView(pDoc);
    BaseView* pAppView = dynamic_cast<BaseView*>(pView);
    if (!pAppView)
    {
        assert(false);
        return;
    }

    pAppView->ortho();
    pAppView->lookAtISO();

    wyap::Scene* pScene = Application::instance().getSceneManager()->getScene(pDoc);
    Scene* pAppScene = dynamic_cast<Scene*>(pScene);
    if (pAppScene)
    {
        pAppView->viewAll(pAppScene->getElementsBoundingBox());
    }
}

wydb::FileType FileCmdsUtil::inferFileType(const std::string& u8FilePath)
{
    const QString suffix = QFileInfo(QString::fromStdString(u8FilePath)).suffix().toLower();
    if (suffix == QString::fromStdString(wy3d::Database::extension(wydb::FileType::Binary)))
    {
        return wydb::FileType::Binary;
    }
    return wydb::FileType::Text;
}

bool FileCmdsUtil::saveFileToPath(wyap::Document* pDoc, const std::string& u8FileName)
{
    assert(pDoc);

    const wydb::FileType fileType = FileCmdsUtil::inferFileType(u8FileName);

    wy::ErrorStatus error = Application::instance().getDocManager()->saveDocument(pDoc, u8FileName, {fileType});
    if (wy::ErrorStatus::Ok == error)
    {
        // Save succeeded: delete the document's .autosave and refresh the autosave time.
        Application::instance().getAutoSave()->onDocumentSaved(pDoc);

        Application::instance().getMainWindow()->setWindowTitle(QString::fromStdString(u8FileName));
        return true;
    }
    else
    {
        // TODO Show an error dialog
        assert(false);
        return false;
    }
}

int NewFileCommand::run()
{
    //// 问询用户是否保存当前文件
    //if (FileCmdsUtil::UiAskRet::Cancel == FileCmdsUtil::uiAskWhetherSaveCurrentFile())
    //{
    //    return 0;
    //}

    // 索引
    static unsigned int index = 0;

    // 默认基础名称
    QString defaultBaseName = QCoreApplication::translate("FileCmds", "unnamed");

    // 新建文档
    QString qstrFileName = QString("%1%2.%3")
        .arg(defaultBaseName)
        .arg(++index)
        .arg(wy3d::Database::extension().c_str());
    std::string strFileFullPath = qstrFileName.toStdString();
    wyap::Document* pDoc = Application::instance().getDocManager()->newDocument(strFileFullPath);
    if (!pDoc)
    {
        assert(false);
        return -1;
    }
    Document* pDocYi3d = dynamic_cast<Document*>(pDoc);
    assert(pDocYi3d);

    // 创建默认的元素
    wydb::Database* pDb = pDoc->getDatabase();
    assert(pDb);
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    assert(pTransMgr);
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (pTrans)
    {
        // added by wangyao 2026.05.15 {
        // 在创建默认元素前设置不响应DbChanged,这样就不会标记DocumentModified.
        if (pDocYi3d)
        {
            pDocYi3d->setDbChangedResponseEnabled(false);
        }
        assert(pDocYi3d->hasStatus(wyap::DocumentStatus::NewlyCreated));
        assert(!pDocYi3d->hasStatus(wyap::DocumentStatus::Modified));
        // }

        //Test::test(pDb, pTrans);
        this->createDefaultElements(pDb, pTrans);
        pTransMgr->endTransaction();
        pTransMgr->clearUndoRedoRecords();

        // added by wangyao 2026.05.15 {
        assert(pDocYi3d->hasStatus(wyap::DocumentStatus::NewlyCreated));
        assert(!pDocYi3d->hasStatus(wyap::DocumentStatus::Modified));
        // 恢复响应DbChanged.
        if (pDocYi3d)
        {
            pDocYi3d->setDbChangedResponseEnabled(true);
        }
        // }
    }
    else
    {
        assert(false);
    }

    //
 /*   wyap::View* pView = Application::instance().getViewManager()->getView(pDoc);
    View* pRealView = dynamic_cast<View*>(pView);
    wyap::Scene* pScene = Application::instance().getSceneManager()->getScene(pDoc);
    Scene* pRealScene = dynamic_cast<Scene*>(pScene);
    if (pRealView && pRealScene)
    {
        osgViewer::View* pOsgView = pRealView->getOsgView();
        if (pOsgView)
        {
            ViewUtil::viewToISO(pOsgView);
            osg::BoundingSphere bsSphere = pRealScene->getElementsBoundingBox();
            ViewUtil::viewAll(pOsgView, bsSphere);
            ViewUtil::ortho(pOsgView);
        }
    }
    else
    {
        assert(false);
    }*/

    // 激活创建的文档
    Application::instance().getDocManager()->activateDocument(
        pDoc, wyap::ExecutionMode::Async);

    //
    wyap::View* pView = Application::instance().getViewManager()->getView(pDoc);
    BaseView* pAppView = dynamic_cast<BaseView*>(pView);
    if (pAppView)
    {
        pAppView->ortho();
        pAppView->lookAtISO();

        wyap::Scene* pScene = Application::instance().getSceneManager()->getScene(pDoc);
        Scene* pAppScene = dynamic_cast<Scene*>(pScene);
        if (pAppScene)
        {
            pAppView->viewAll(pAppScene->getElementsBoundingBox());
        }
    }
    //// 刷新窗口标题
    //Application::instance().getMainWindow()->setWindowTitle(QString::fromStdString(pDoc->getFileName()));

    //Application::instance().getCmdManager()->postCommand(CommandNames::IsometricView);
    //Application::instance().getCmdManager()->postCommand(CommandNames::FitView);
    //Application::instance().getCmdManager()->postCommand(CommandNames::OrthoCamera);
    //Application::instance().getCmdManager()->postCommand(CommandNames::MakeBox);

    //Application::instance().getViewManager()->getView(pDoc);

    /*
    // 调整视图
    Application::instance().viewISO();
    Application::instance().fitView();

    // 正交视图
    View* pActiveView = Application::instance().getActiveView();
    if (pActiveView)
    {
        ViewUtil::ortho(pActiveView->getOsgView());
    }
    */

    return 0;
}

void NewFileCommand::createDefaultElements(wydb::Database* pDb, wydb::Transaction* pTrans)
{
    assert(pDb);
    assert(pTrans);

    auto createDatumPlane = [pDb, pTrans](const wy::Vector3& origin,
        const wy::Vector3& zDir, const wy::Vector3& xDir, const std::string& name) -> bool
    {
        wy3d::DatumPlane* pDatumPlane(nullptr);
        wy3d::SketchPlane sketchPlane(origin, zDir, xDir);
        if (!sketchPlane.isValid()) return false;
        wy::ErrorStatus error = wy3d::DatumPlane::create(pTrans, sketchPlane, pDatumPlane);
        if (wy::ErrorStatus::Ok != error)
        {
            return false;
        }
        assert(pDatumPlane);
        error = pDatumPlane->setName(name);
        assert(wy::ErrorStatus::Ok == error);
        return true;
    };

    bool ret(false);
    // 俯视基准面
    ret = createDatumPlane(wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis, "TOP");
    assert(ret);
    // 右视基准面
    ret = createDatumPlane(wy::Vector3::kZero, wy::Vector3::kXAxis, wy::Vector3::kYAxis, "RIGHT");
    assert(ret);
    // 前视基准面
    ret = createDatumPlane(wy::Vector3::kZero, -wy::Vector3::kYAxis, wy::Vector3::kXAxis, "FRONT");
    assert(ret);
}

int OpenFileCommand::run()
{
    // 打开文件对话框
    const QString textExt = QString::fromStdString(wy3d::Database::extension(wydb::FileType::Text));
    const QString binaryExt = QString::fromStdString(wy3d::Database::extension(wydb::FileType::Binary));
    const QString allFilter = QCoreApplication::translate("FileCmds", "All YI3D files (*.%1 *.%2)").arg(textExt, binaryExt);
    const QString textFilter = QCoreApplication::translate("FileCmds", "YI3D Text files (*.%1)").arg(textExt);
    const QString binaryFilter = QCoreApplication::translate("FileCmds", "YI3D Binary files (*.%1)").arg(binaryExt);
    const QString filter = allFilter + ";;" + textFilter + ";;" + binaryFilter;

    QString fileFullPath = QFileDialog::getOpenFileName(Application::instance().getMainWindow(),
        QCoreApplication::translate("FileCmds", "Open file"), "", filter,
        0, QFileDialog::DontUseNativeDialog);
    if (fileFullPath.isEmpty() || fileFullPath.isNull())
    {
        return 0;
    }
    std::string u8FileFullPath = fileFullPath.toStdString();

    // 如果要打开的文件就是当前文件
    bool isOpenActiveFile(false);
    wyap::Document* pActiveDoc = Application::instance().getDocManager()->getActiveDocument();
    if (pActiveDoc)
    {
        if (!pActiveDoc->hasStatus(wyap::DocumentStatus::NewlyCreated) && pActiveDoc->getFileName() == u8FileFullPath)
        {
            isOpenActiveFile = true;
            if (!pActiveDoc->hasStatus(wyap::DocumentStatus::Modified)) // 文件没有更改直接返回
            {
                return 0;
            }
        }
    }

    // 打开当前文件
    if (isOpenActiveFile)
    {
        // 您要返回到存储版本吗？
        QString text = QCoreApplication::translate("FileCmds", "Do you want to revert to the saved version?")
            + "\n" + QString::fromStdString(u8FileFullPath);
        int ret = QMessageBox::question(Application::instance().getMainWindow(), "Yi3D", text,
            QCoreApplication::translate("FileCmds", "Yes"),
            QCoreApplication::translate("FileCmds", "No"));
        if (0 == ret) // Yes
        {
            // 继续后面打开文档的流程
        }
        else if (1 == ret) // No
        {
            return 0;
        }
        else // other
        {
            assert(false);
            return 0;
        }
    }

    return openFile(u8FileFullPath);
}

int OpenFileCommand::openFile(const std::string& u8FileFullPath)
{
    // Crash recovery check: if a .autosave exists next to the file, ask the
    // user to recover or discard. Returns true when the recovery flow already
    // handled the open, in which case opening the original is skipped.
    if (Application::instance().getAutoSave()->checkRecoveryForOpen(u8FileFullPath))
    {
        return 0;
    }

    // 根据文件后缀推断文件类型
    wydb::FileType fileType = FileCmdsUtil::inferFileType(u8FileFullPath);
    wydb::Database::ReadFileOption readFileOption;
    readFileOption.fileType = fileType;

    // 打开文档
    wyap::Document* pDoc(nullptr);
    wy::ErrorStatus error = Application::instance().getDocManager()->openDocument(
        u8FileFullPath, readFileOption, pDoc);
    if (wy::ErrorStatus::Ok != error)
    {
        MessageBoxUtil::showOpenFileError(error);
        return 0;
    }
    if (!pDoc)
    {
        assert(false);
        return 0;
    }

    // 激活打开的文档
    error = Application::instance().getDocManager()->activateDocument(
        pDoc, wyap::ExecutionMode::Async);

    // Fit the view: orthographic + isometric + zoom to the model.
    FileCmdsUtil::fitViewToAll(pDoc);

    return 0;
}

int SaveFileCommand::run()
{
    wyap::Document* pActiveDoc = Application::instance().getActiveDocument();
    if (!pActiveDoc)
    {
        assert(false);
        return 0;
    }

    bool isUserCanceled(false);
    FileCmdsUtil::uiSaveFile(pActiveDoc, false, isUserCanceled);
    return 0;
}

int SaveAsFileCommand::run()
{
    wyap::Document* pActiveDoc = Application::instance().getActiveDocument();
    if (!pActiveDoc)
    {
        assert(false);
        return 0;
    }

    bool isUserCanceled(false);
    FileCmdsUtil::uiSaveFile(pActiveDoc, true, isUserCanceled);
    return 0;
}

int ExportFileCommand::run()
{
    // 当前文档名
    wyap::Document* pActiveDoc = Application::instance().getDocManager()->getActiveDocument();
    if (!pActiveDoc) return -1;
    std::string u8ActiveDocFileName = pActiveDoc->getFileName();

    // 去掉文件后缀
    size_t lastDot = u8ActiveDocFileName.find_last_of('.');
    if (lastDot != std::string::npos && lastDot != 0)
    {
        u8ActiveDocFileName = u8ActiveDocFileName.substr(0, lastDot);
    }

    // 所有导出器
    const std::map<QString, std::shared_ptr<Exporter>>& allExporters = ExporterManager::instance().getAllExporters();

    // filter list
    QStringList filterList;
    for (const auto& kvp : allExporters)
    {
        filterList << kvp.first;
    }
    QString filter = filterList.join(";;");

    // dialog
    QString selectedFilter;
    QString fileName = QFileDialog::getSaveFileName(
        Application::instance().getMainWindow(),
        QCoreApplication::translate("FileCmds", "Export file"),
        QString::fromStdString(u8ActiveDocFileName),
        filter, &selectedFilter, 0);
    if (fileName.isEmpty())
    {
        return 0; // 用户取消的情况下应该返回0
    }

    // export
    auto iter = allExporters.find(selectedFilter);
    if (iter == allExporters.cend())
    {
        assert(false);
        return -1;
    }
    if (!iter->second)
    {
        assert(false);
        return -1;
    }
    bool ret = iter->second->perform(pActiveDoc->getDatabase(), fileName.toStdWString());
    if (!ret)
    {
        MessageBoxUtil::showError(QCoreApplication::translate("FileCmds", "Export file failed!"));
    }

    return 0;
}

int ExportSelectedCommand::run()
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return -1;
    }

    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    if (ss.isEmpty())
    {
        assert(false);
        return -1;
    }

    // Collect shapes from selected Solid/Sheet elements
    std::vector<TopoDS_Shape> shapes;
    shapes.reserve(10);
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wydb::Element* pElem = pDb->getElement(iter.current().getElementId());
        if (!pElem) continue;
        if (const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem))
        {
            const TopoDS_Shape& shape = pSolid->getShape();
            if (!shape.IsNull()) shapes.push_back(shape);
        }
        else if (const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pElem))
        {
            const TopoDS_Shape& shape = pSheet->getShape();
            if (!shape.IsNull()) shapes.push_back(shape);
        }
    }
    if (shapes.empty())
    {
        assert(false);
        return -1;
    }

    // Build compound for multi-selection
    TopoDS_Shape exportShape;
    if (shapes.size() == 1)
        exportShape = shapes.front();
    else
    {
        TopoDS_Compound compound;
        BRep_Builder builder;
        builder.MakeCompound(compound);
        for (const auto& shape : shapes)
            if (!shape.IsNull()) builder.Add(compound, shape);
        exportShape = compound;
    }

    // 所有导出器
    const std::map<QString, std::shared_ptr<Exporter>>& allExporters = ExporterManager::instance().getAllExporters();

    // filter list
    QStringList filterList;
    for (const auto& kvp : allExporters)
        filterList << kvp.first;
    QString filter = filterList.join(";;");

    // dialog
    QString selectedFilter;
    QString fileName = QFileDialog::getSaveFileName(
        Application::instance().getMainWindow(),
        QCoreApplication::translate("FileCmds", "Export"),
        "", filter, &selectedFilter, 0);
    if (fileName.isEmpty()) return 0;

    // export
    auto it = allExporters.find(selectedFilter);
    if (it == allExporters.cend() || !it->second) { assert(false); return -1; }

    bool ret = it->second->perform(exportShape, fileName.toStdWString());
    if (!ret)
        MessageBoxUtil::showError(QCoreApplication::translate("FileCmds", "Export failed!"));

    return 0;
}

int ImportFileCommand::run()
{
    // 当前数据库
    wyap::Document* pActiveDoc = Application::instance().getDocManager()->getActiveDocument();
    if (!pActiveDoc) return -1;
    wydb::Database* pDb = pActiveDoc->getDatabase();
    if (!pDb) return -1;

    // 所有导入器
    const std::map<QString, std::shared_ptr<Importer>>& allImporters = ImporterManager::instance().getAllImporters();
    if (allImporters.empty())
    {
        assert(false);
        return -1;
    }

    // filter list
    QStringList filterList;
    for (const auto& kvp : allImporters)
    {
        filterList << kvp.first;
    }
    QString filter = filterList.join(";;");

    // 导入文件对话框
    QString selectedFilter;
    QString fileFullPath = QFileDialog::getOpenFileName(Application::instance().getMainWindow(),
        QCoreApplication::translate("FileCmds", "Import file"), "", filter, &selectedFilter, QFileDialog::DontUseNativeDialog);
    if (fileFullPath.isEmpty() || fileFullPath.isNull())
    {
        return 0;
    }
    std::wstring wstrFileFullPath = fileFullPath.toStdWString();

    // 导入文件
    auto iter = allImporters.find(selectedFilter);
    if (iter == allImporters.cend())
    {
        assert(false);
        return -1;
    }
    if (!iter->second)
    {
        assert(false);
        return -1;
    }
    bool ret = iter->second->perform(pDb, wstrFileFullPath);
    if (!ret)
    {
        MessageBoxUtil::showError(QCoreApplication::translate("FileCmds", "Import file failed!"));
    }

    return 0;
}

int RunScriptCommand::run()
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return -1;
    }

    // 交互选择脚本文件
    QStringList filterList;
    filterList << QCoreApplication::translate("RunScriptCommand", "Python Script (*.py)");
    QString filter = filterList.join(";;");
    QString selectedFilter;
    QString fileFullPath = QFileDialog::getOpenFileName(Application::instance().getMainWindow(),
        QCoreApplication::translate("RunScriptCommand", "Run Python Script"), "", filter, &selectedFilter, QFileDialog::DontUseNativeDialog);
    if (fileFullPath.isEmpty() || fileFullPath.isNull())
    {
        return 0;
    }

    std::string strFileFullPath = fileFullPath.toUtf8().constData();
    PythonScriptExecutor scriptExecutor;
    PythonScriptExecutor::Error error = scriptExecutor.Run(strFileFullPath);
    return 0;
}

void RunScriptCommand::abortActiveTransaction()
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;

    // 如果有激活的事务则取消事务
    if (pDb->getTransactionManager()->getActiveTransaction())
    {
        pDb->getTransactionManager()->abortTransaction();
    }
}

