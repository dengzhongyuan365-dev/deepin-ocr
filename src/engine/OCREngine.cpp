// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "OCREngine.h"
#include "OCRDynamicLoader.h"
#include <QFileInfo>
#include "util/log.h"

OCREngine::OCREngine()
{
    //初始化变量
    m_isRunning = false;

    //初始化OCR动态加载器
    //此处存在产品设计缺陷: 无法选择插件，无鉴权入口，无性能方面的高级设置入口
    //因此此处直接硬编码使用默认插件
    qCInfo(dmOcr) << "正在初始化OCR动态加载器";
    ocrLoader = new OCRDynamicLoader(); // 创建局部实例而不是使用单例
    
    if (!ocrLoader->loadLibrary()) {
        qCCritical(dmOcr) << "无法加载OCR库:" << ocrLoader->getLastError();
        qCCritical(dmOcr) << "请确保已正确安装libdtkocr开发包";
        return;
    }
    
    qCInfo(dmOcr) << "OCR库加载成功，版本:" << ocrLoader->getVersion();
    
    if (!ocrLoader->createOCR()) {
        qCCritical(dmOcr) << "无法创建OCR实例:" << ocrLoader->getLastError();
        return;
    }
    
    if (!ocrLoader->loadDefaultPlugin()) {
        qCCritical(dmOcr) << "无法加载默认插件:" << ocrLoader->getLastError();
        return;
    }
    
    // 等待插件准备完成
    if (!ocrLoader->pluginReady()) {
        qCWarning(dmOcr) << "OCR插件尚未准备就绪";
    }
    
    // 设置默认线程数
    if (!ocrLoader->setMaxThreads(2)) {
        qCWarning(dmOcr) << "设置最大线程数失败:" << ocrLoader->getLastError();
    }
    
    // 检查GPU硬件加速
    QFileInfo mtfi("/dev/mtgpu.0");
    if (mtfi.exists()) {
        qCInfo(dmOcr) << "检测到GPU设备，启用Vulkan硬件加速";
        if (!ocrLoader->setHardware(HARDWARE_GPU_VULKAN, 0)) {
            qCWarning(dmOcr) << "启用GPU加速失败:" << ocrLoader->getLastError();
            qCInfo(dmOcr) << "回退使用CPU处理";
        } else {
            qCInfo(dmOcr) << "GPU硬件加速启用成功";
        }
    } else {
        qCInfo(dmOcr) << "未检测到GPU设备，使用CPU处理";
    }
    
    qCInfo(dmOcr) << "OCR引擎初始化完成";
}

OCREngine::~OCREngine()
{
    qCInfo(dmOcr) << "OCR引擎析构，清理资源";
    
    // 清理OCR动态加载器
    if (ocrLoader) {
        ocrLoader->destroyOCR();
        ocrLoader->unloadLibrary();
        delete ocrLoader;
        ocrLoader = nullptr;
    }
    
    qCInfo(dmOcr) << "OCR引擎资源清理完成";
}

void OCREngine::setImage(const QImage &image)
{
    if (!ocrLoader || !ocrLoader->isLoaded()) {
        qCWarning(dmOcr) << "OCR加载器不可用";
        return;
    }
    
    if (image.isNull()) {
        qCWarning(dmOcr) << "输入图像为空";
        return;
    }
    
    qCDebug(dmOcr) << "设置OCR输入图像，尺寸:" << image.size() << "格式:" << image.format();
    
    auto inputImage = image.convertToFormat(QImage::Format_RGB888);
    if (!ocrLoader->setImage(inputImage)) {
        qCWarning(dmOcr) << "设置图像数据失败:" << ocrLoader->getLastError();
    }
}

QString OCREngine::getRecogitionResult()
{
    if (!ocrLoader || !ocrLoader->isLoaded()) {
        qCWarning(dmOcr) << "OCR加载器不可用";
        return QString();
    }
    
    if (!ocrLoader->pluginReady()) {
        qCWarning(dmOcr) << "OCR插件尚未准备就绪";
        return QString();
    }
    
    qCInfo(dmOcr) << "开始OCR文字识别";
    m_isRunning = true;

    bool success = ocrLoader->analyze();
    if (!success) {
        qCWarning(dmOcr) << "OCR分析失败:" << ocrLoader->getLastError();
        m_isRunning = false;
        return QString();
    }

    m_isRunning = false;
    QString result = ocrLoader->getSimpleResult();
    qCInfo(dmOcr) << "OCR识别完成，结果长度:" << result.length();
    
    if (result.isEmpty()) {
        qCInfo(dmOcr) << "未识别到文字内容";
    }
    
    return result;
}

bool OCREngine::setLanguage(const QString &language)
{
    if (!ocrLoader || !ocrLoader->isLoaded()) {
        qCWarning(dmOcr) << "OCR加载器不可用";
        return false;
    }
    
    if (language.isEmpty()) {
        qCWarning(dmOcr) << "语言参数为空";
        return false;
    }
    
    qCInfo(dmOcr) << "设置OCR识别语言为:" << language;
    
    // 如果正在运行，先中断当前分析
    if(ocrLoader->isRunning()) {
        qCInfo(dmOcr) << "中断当前分析以切换语言";
        if (!ocrLoader->breakAnalyze()) {
            qCWarning(dmOcr) << "中断分析失败:" << ocrLoader->getLastError();
        }
    }

    bool success = ocrLoader->setLanguage(language);
    if (!success) {
        qCWarning(dmOcr) << "设置语言失败:" << language << "错误:" << ocrLoader->getLastError();
    } else {
        qCInfo(dmOcr) << "语言设置成功:" << language;
    }
    
    return success;
}
