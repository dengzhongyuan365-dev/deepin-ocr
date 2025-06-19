// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ocrapplication.h"
#include "mainwindow.h"
#include "engine/OCREngine.h"
#include <DWidgetUtil>
#include "util/log.h"

OcrApplication::OcrApplication(QObject *parent) : QObject(parent)
{

}

bool OcrApplication::openFile(QString filePath)
{
    qCInfo(dmOcr) << __FUNCTION__ << __LINE__ << filePath;
    bool bRet = false;
    
    // 直接创建MainWindow，OCREngine现在是成员变量，不需要检查全局状态
    MainWindow *win = new MainWindow(nullptr);
    
    //增加判断，空图片不会启动
    bRet = win->openFile(filePath);
    if (bRet) {
        win->show();
        //第一次启动才居中
        if (m_loadingCount == 0) {
            Dtk::Widget::moveToCenter(win);
            m_loadingCount++;
            qCDebug(dmOcr) << "First launch, centering window";
        }
    } else {
        qCWarning(dmOcr) << "Failed to open file:" << filePath;
        win->deleteLater(); // 清理窗口
        win = nullptr;
    }

    return bRet;
}

void OcrApplication::openImage(QImage image)
{
    //增加判断，空图片不会启动
    if (!image.isNull() && image.width() >= 1) {
        qCInfo(dmOcr) << "Opening image, size:" << image.size();
        
        // 直接创建MainWindow，OCREngine现在是成员变量，不需要检查全局状态
        MainWindow *win = new MainWindow(nullptr);
        
        win->openImage(image);
        win->show();
        //第一次启动才居中
        if (m_loadingCount == 0) {
            Dtk::Widget::moveToCenter(win);
            m_loadingCount++;
            qCDebug(dmOcr) << "First launch, centering window";
        }
    } else {
        qCWarning(dmOcr) << "Invalid image: null or width < 1";
    }
}

void OcrApplication::openImageAndName(QImage image, QString imageName)
{
    //增加判断，空图片不会启动
    if (!image.isNull() && image.width() >= 1) {
        qCInfo(dmOcr) << "Opening image with name:" << imageName << ", size:" << image.size();
        
        // 直接创建MainWindow，OCREngine现在是成员变量，不需要检查全局状态
        MainWindow *win = new MainWindow(nullptr);
        
        win->openImage(image, imageName);
        win->show();
        //第一次启动才居中
        if (m_loadingCount == 0) {
            Dtk::Widget::moveToCenter(win);
            m_loadingCount++;
            qCDebug(dmOcr) << "First launch, centering window";
        }
    } else {
        qCWarning(dmOcr) << "Invalid image: null or width < 1";
    }
}

