// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "OCRDynamicLoader.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QLibraryInfo>
#include <QStandardPaths>
#include <QProcess>
#include <QCoreApplication>

// 最小兼容版本
#define MIN_API_VERSION_MAJOR 1
#define MIN_API_VERSION_MINOR 0
#define MIN_API_VERSION_PATCH 0

OCRDynamicLoader::OCRDynamicLoader()
    : m_library(nullptr)
    , m_ocrHandle(nullptr)
    , m_ocr_create(nullptr)
    , m_ocr_destroy(nullptr)
    , m_ocr_load_default_plugin(nullptr)
    , m_ocr_plugin_ready(nullptr)
    , m_ocr_set_hardware(nullptr)
    , m_ocr_set_max_threads(nullptr)
    , m_ocr_set_language(nullptr)
    , m_ocr_set_image_file(nullptr)
    , m_ocr_set_image_data(nullptr)
    , m_ocr_analyze(nullptr)
    , m_ocr_break_analyze(nullptr)
    , m_ocr_is_running(nullptr)
    , m_ocr_get_simple_result(nullptr)
    , m_ocr_get_text_boxes(nullptr)
    , m_ocr_free_text_boxes(nullptr)
    , m_ocr_get_version(nullptr)
    , m_ocr_get_api_version_major(nullptr)
    , m_ocr_get_api_version_minor(nullptr)
    , m_ocr_get_api_version_patch(nullptr)
{
    qDebug() << "OCRDynamicLoader: 初始化动态加载器";
}

OCRDynamicLoader::~OCRDynamicLoader()
{
    unloadLibrary();
}

bool OCRDynamicLoader::loadLibrary()
{
    if (m_library && m_library->isLoaded()) {
        qDebug() << "OCRDynamicLoader: 库已加载";
        return true;
    }
    
    QString libraryPath = findLibraryFile();
    if (libraryPath.isEmpty()) {
        setError("未找到dtk6ocr库文件");
        return false;
    }
    
    qDebug() << "OCRDynamicLoader: 尝试加载库:" << libraryPath;
    
    m_library = std::make_unique<QLibrary>(libraryPath);
    if (!m_library->load()) {
        setError(QString("加载库失败: %1").arg(m_library->errorString()));
        m_library.reset();
        return false;
    }
    
    qDebug() << "OCRDynamicLoader: 库加载成功，开始加载函数";
    
    if (!loadFunctions()) {
        unloadLibrary();
        return false;
    }
    
    // 加载版本检查函数
    if (!loadVersionFunctions()) {
        qWarning() << "OCRDynamicLoader: 版本检查函数加载失败，使用基础兼容模式";
    }
    
    // 检查API兼容性
    if (!isAPICompatible()) {
        setError("API版本不兼容");
        unloadLibrary();
        return false;
    }
    
    qDebug() << "OCRDynamicLoader: 动态加载成功" << getAPIVersionInfo();
    return true;
}

QStringList OCRDynamicLoader::getSystemLibraryPaths()
{
    QStringList paths;
    
    // 标准库路径
    paths << "/usr/lib"
          << "/usr/local/lib"
          << "/lib";
    
    // 多架构路径
    QString arch = QSysInfo::currentCpuArchitecture();
    if (arch == "x86_64") {
        paths << "/usr/lib/x86_64-linux-gnu"
              << "/usr/local/lib/x86_64-linux-gnu"
              << "/lib/x86_64-linux-gnu";
    } else if (arch.startsWith("arm")) {
        if (arch.contains("64")) {
            paths << "/usr/lib/aarch64-linux-gnu"
                  << "/usr/local/lib/aarch64-linux-gnu"
                  << "/lib/aarch64-linux-gnu";
        } else {
            paths << "/usr/lib/arm-linux-gnueabihf"
                  << "/usr/local/lib/arm-linux-gnueabihf"
                  << "/lib/arm-linux-gnueabihf";
        }
    }
    
    // LD_LIBRARY_PATH环境变量
    QByteArray ldPath = qgetenv("LD_LIBRARY_PATH");
    if (!ldPath.isEmpty()) {
        QStringList envPaths = QString::fromLocal8Bit(ldPath).split(':', Qt::SkipEmptyParts);
        paths.append(envPaths);
    }
    
    // Qt库路径 - 兼容Qt5和Qt6
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    paths << QLibraryInfo::path(QLibraryInfo::LibrariesPath);
#else
    paths << QLibraryInfo::location(QLibraryInfo::LibrariesPath);
#endif
    
    return paths;
}

QStringList OCRDynamicLoader::getLibraryNames()
{
    return QStringList() 
        << "libdtk6ocr.so.1.0.0"  // 精确版本
        << "libdtk6ocr.so.1"      // 主版本
        << "libdtk6ocr.so"        // 无版本
        << "libdtkocr.so"         // 向后兼容名称
        << "dtk6ocr"              // QLibrary自动添加前缀和扩展名
        << "dtkocr";              // 向后兼容
}

QString OCRDynamicLoader::findLibraryFile()
{
    QStringList paths = getSystemLibraryPaths();
    QStringList names = getLibraryNames();
    
    qDebug() << "OCRDynamicLoader: 搜索库路径:" << paths.size() << "个";
    qDebug() << "OCRDynamicLoader: 搜索库名称:" << names;
    
    // 首先尝试使用QLibrary的自动搜索
    for (const QString& name : names) {
        QLibrary testLib(name);
        if (testLib.load()) {
            QString fileName = testLib.fileName();
            testLib.unload();
            qDebug() << "OCRDynamicLoader: 找到库(自动搜索):" << fileName;
            return name; // 返回库名，让QLibrary自己处理路径
        }
    }
    
    // 手动搜索特定路径
    for (const QString& path : paths) {
        QDir dir(path);
        if (!dir.exists()) continue;
        
        for (const QString& name : names) {
            QString fullPath = dir.absoluteFilePath(name);
            QFileInfo fileInfo(fullPath);
            
            if (fileInfo.exists() && fileInfo.isReadable()) {
                qDebug() << "OCRDynamicLoader: 找到库文件:" << fullPath;
                return fullPath;
            }
        }
    }
    
    qWarning() << "OCRDynamicLoader: 未找到任何dtk6ocr库文件";
    return QString();
}

bool OCRDynamicLoader::loadFunctions()
{
    // 必需函数加载
    struct FunctionInfo {
        void** ptr;
        const char* name;
        bool required;
    };
    
    FunctionInfo functions[] = {
        { (void**)&m_ocr_create, "ocr_create", true },
        { (void**)&m_ocr_destroy, "ocr_destroy", true },
        { (void**)&m_ocr_load_default_plugin, "ocr_load_default_plugin", true },
        { (void**)&m_ocr_plugin_ready, "ocr_plugin_ready", true },
        { (void**)&m_ocr_set_hardware, "ocr_set_hardware", false },
        { (void**)&m_ocr_set_max_threads, "ocr_set_max_threads", false },
        { (void**)&m_ocr_set_language, "ocr_set_language", false },
        { (void**)&m_ocr_set_image_file, "ocr_set_image_file", true },
        { (void**)&m_ocr_set_image_data, "ocr_set_image_data", true },
        { (void**)&m_ocr_analyze, "ocr_analyze", true },
        { (void**)&m_ocr_break_analyze, "ocr_break_analyze", false },
        { (void**)&m_ocr_is_running, "ocr_is_running", false },
        { (void**)&m_ocr_get_simple_result, "ocr_get_simple_result", true },
        { (void**)&m_ocr_get_text_boxes, "ocr_get_text_boxes", false },
        { (void**)&m_ocr_free_text_boxes, "ocr_free_text_boxes", false },
        { (void**)&m_ocr_get_version, "ocr_get_version", false }
    };
    
    int loadedCount = 0;
    int requiredCount = 0;
    
    for (const auto& func : functions) {
        *(func.ptr) = reinterpret_cast<void*>(m_library->resolve(func.name));
        
        if (*(func.ptr)) {
            loadedCount++;
            qDebug() << "OCRDynamicLoader: 成功加载函数" << func.name;
        } else {
            if (func.required) {
                setError(QString("加载必需函数失败: %1").arg(func.name));
                return false;
            }
            qWarning() << "OCRDynamicLoader: 可选函数未找到:" << func.name;
        }
        
        if (func.required) requiredCount++;
    }
    
    qDebug() << QString("OCRDynamicLoader: 加载函数 %1/%2 (必需: %3)")
                .arg(loadedCount).arg(sizeof(functions)/sizeof(FunctionInfo)).arg(requiredCount);
    
    return true;
}

bool OCRDynamicLoader::loadVersionFunctions()
{
    m_ocr_get_api_version_major = (ocr_get_api_version_major_func)m_library->resolve("ocr_get_api_version_major");
    m_ocr_get_api_version_minor = (ocr_get_api_version_minor_func)m_library->resolve("ocr_get_api_version_minor");
    m_ocr_get_api_version_patch = (ocr_get_api_version_patch_func)m_library->resolve("ocr_get_api_version_patch");
    
    bool hasVersionFunctions = m_ocr_get_api_version_major && 
                              m_ocr_get_api_version_minor && 
                              m_ocr_get_api_version_patch;
    
    if (hasVersionFunctions) {
        qDebug() << "OCRDynamicLoader: 版本检查函数加载成功";
    }
    
    return hasVersionFunctions;
}

bool OCRDynamicLoader::isAPICompatible() const
{
    if (!m_ocr_get_api_version_major || !m_ocr_get_api_version_minor || !m_ocr_get_api_version_patch) {
        qWarning() << "OCRDynamicLoader: 无版本信息，假设兼容";
        return true; // 向后兼容旧版本
    }
    
    int major = m_ocr_get_api_version_major();
    int minor = m_ocr_get_api_version_minor();
    int patch = m_ocr_get_api_version_patch();
    
    qDebug() << QString("OCRDynamicLoader: 检查API版本 %1.%2.%3 >= %4.%5.%6")
                .arg(major).arg(minor).arg(patch)
                .arg(MIN_API_VERSION_MAJOR).arg(MIN_API_VERSION_MINOR).arg(MIN_API_VERSION_PATCH);
    
    // 检查主版本
    if (major > MIN_API_VERSION_MAJOR) return true;
    if (major < MIN_API_VERSION_MAJOR) return false;
    
    // 主版本相同，检查次版本
    if (minor > MIN_API_VERSION_MINOR) return true;
    if (minor < MIN_API_VERSION_MINOR) return false;
    
    // 次版本相同，检查补丁版本
    return patch >= MIN_API_VERSION_PATCH;
}

QString OCRDynamicLoader::getAPIVersionInfo() const
{
    if (!m_ocr_get_api_version_major || !m_ocr_get_api_version_minor || !m_ocr_get_api_version_patch) {
        if (m_ocr_get_version) {
            return QString("库版本: %1 (无API版本信息)").arg(m_ocr_get_version());
        }
        return "版本信息不可用";
    }
    
    int major = m_ocr_get_api_version_major();
    int minor = m_ocr_get_api_version_minor();
    int patch = m_ocr_get_api_version_patch();
    
    QString info = QString("API版本: %1.%2.%3").arg(major).arg(minor).arg(patch);
    
    if (m_ocr_get_version) {
        info += QString(" (库版本: %1)").arg(m_ocr_get_version());
    }
    
    return info;
}

void OCRDynamicLoader::unloadLibrary()
{
    if (m_ocrHandle) {
        destroyOCR();
    }
    
    if (m_library && m_library->isLoaded()) {
        m_library->unload();
        qDebug() << "OCRDynamicLoader: 库已卸载";
    }
    
    m_library.reset();
    
    // 重置所有函数指针
    m_ocr_create = nullptr;
    m_ocr_destroy = nullptr;
    m_ocr_load_default_plugin = nullptr;
    m_ocr_plugin_ready = nullptr;
    m_ocr_set_hardware = nullptr;
    m_ocr_set_max_threads = nullptr;
    m_ocr_set_language = nullptr;
    m_ocr_set_image_file = nullptr;
    m_ocr_set_image_data = nullptr;
    m_ocr_analyze = nullptr;
    m_ocr_break_analyze = nullptr;
    m_ocr_is_running = nullptr;
    m_ocr_get_simple_result = nullptr;
    m_ocr_get_text_boxes = nullptr;
    m_ocr_free_text_boxes = nullptr;
    m_ocr_get_version = nullptr;
    m_ocr_get_api_version_major = nullptr;
    m_ocr_get_api_version_minor = nullptr;
    m_ocr_get_api_version_patch = nullptr;
    
    m_lastError.clear();
}

bool OCRDynamicLoader::isLoaded() const
{
    return m_library && m_library->isLoaded();
}

bool OCRDynamicLoader::createOCR()
{
    if (m_ocrHandle) {
        qDebug() << "OCRDynamicLoader: OCR实例已存在";
        return true;
    }
    
    if (!m_ocr_create) {
        setError("ocr_create函数未加载");
        return false;
    }
    
    m_ocrHandle = m_ocr_create();
    if (!m_ocrHandle) {
        setError("创建OCR实例失败");
        return false;
    }
    
    qDebug() << "OCRDynamicLoader: OCR实例创建成功";
    return true;
}

void OCRDynamicLoader::destroyOCR()
{
    if (m_ocrHandle && m_ocr_destroy) {
        m_ocr_destroy(m_ocrHandle);
        m_ocrHandle = nullptr;
        qDebug() << "OCRDynamicLoader: OCR实例已销毁";
    }
}

bool OCRDynamicLoader::loadDefaultPlugin()
{
    if (!m_ocrHandle) {
        setError("OCR实例未创建");
        return false;
    }
    
    if (!m_ocr_load_default_plugin) {
        setError("ocr_load_default_plugin函数未加载");
        return false;
    }
    
    int result = m_ocr_load_default_plugin(m_ocrHandle);
    if (result == 0) {
        setError(QString("加载默认插件失败，错误代码: %1").arg(result));
        return false;
    }
    
    qDebug() << "OCRDynamicLoader: 默认插件加载成功";
    return true;
}

bool OCRDynamicLoader::pluginReady() const
{
    if (!m_ocrHandle || !m_ocr_plugin_ready) {
        return false;
    }
    
    return m_ocr_plugin_ready(m_ocrHandle) != 0;
}

bool OCRDynamicLoader::setHardware(HardwareType type, int deviceId)
{
    if (!m_ocrHandle) {
        setError("OCR实例未创建");
        return false;
    }
    
    if (!m_ocr_set_hardware) {
        qWarning() << "OCRDynamicLoader: setHardware函数不可用，跳过";
        return true; // 可选功能，不算错误
    }
    
    int result = m_ocr_set_hardware(m_ocrHandle, type, deviceId);
    if (result == 0) {
        setError(QString("设置硬件类型失败，错误代码: %1").arg(result));
        return false;
    }
    
    qDebug() << "OCRDynamicLoader: 硬件设置成功，类型:" << type << "设备ID:" << deviceId;
    return true;
}

bool OCRDynamicLoader::setMaxThreads(int count)
{
    if (!m_ocrHandle) {
        setError("OCR实例未创建");
        return false;
    }
    
    if (!m_ocr_set_max_threads) {
        qWarning() << "OCRDynamicLoader: setMaxThreads函数不可用，跳过";
        return true;
    }
    
    int result = m_ocr_set_max_threads(m_ocrHandle, count);
    if (result == 0) {
        setError(QString("设置最大线程数失败，错误代码: %1").arg(result));
        return false;
    }
    
    qDebug() << "OCRDynamicLoader: 最大线程数设置为:" << count;
    return true;
}

bool OCRDynamicLoader::setLanguage(const QString& language)
{
    if (!m_ocrHandle) {
        setError("OCR实例未创建");
        return false;
    }
    
    if (!m_ocr_set_language) {
        qWarning() << "OCRDynamicLoader: setLanguage函数不可用，跳过";
        return true;
    }
    
    QByteArray langBytes = language.toUtf8();
    int result = m_ocr_set_language(m_ocrHandle, langBytes.constData());
    if (result == 0) {
        setError(QString("设置语言失败，错误代码: %1").arg(result));
        return false;
    }
    
    qDebug() << "OCRDynamicLoader: 语言设置为:" << language;
    return true;
}

bool OCRDynamicLoader::setImageFile(const QString& filePath)
{
    if (!m_ocrHandle) {
        setError("OCR实例未创建");
        return false;
    }
    
    if (!m_ocr_set_image_file) {
        setError("ocr_set_image_file函数未加载");
        return false;
    }
    
    QByteArray pathBytes = filePath.toUtf8();
    int result = m_ocr_set_image_file(m_ocrHandle, pathBytes.constData());
    if (result == 0) {
        setError(QString("设置图像文件失败，错误代码: %1").arg(result));
        return false;
    }
    
    qDebug() << "OCRDynamicLoader: 图像文件设置成功:" << filePath;
    return true;
}

bool OCRDynamicLoader::setImage(const QImage& image)
{
    if (!m_ocrHandle) {
        setError("OCR实例未创建");
        return false;
    }
    
    if (!m_ocr_set_image_data) {
        setError("ocr_set_image_data函数未加载");
        return false;
    }
    
    if (image.isNull()) {
        setError("图像数据无效");
        return false;
    }
    
    // 转换为RGB格式
    QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);
    
    int result = m_ocr_set_image_data(m_ocrHandle, 
                                     rgbImage.constBits(),
                                     rgbImage.width(),
                                     rgbImage.height(),
                                     3); // RGB = 3 channels
    if (result == 0) {
        setError(QString("设置图像数据失败，错误代码: %1").arg(result));
        return false;
    }
    
    qDebug() << QString("OCRDynamicLoader: 图像数据设置成功 %1x%2").arg(rgbImage.width()).arg(rgbImage.height());
    return true;
}

bool OCRDynamicLoader::analyze()
{
    if (!m_ocrHandle) {
        setError("OCR实例未创建");
        return false;
    }
    
    if (!m_ocr_analyze) {
        setError("ocr_analyze函数未加载");
        return false;
    }
    
    int result = m_ocr_analyze(m_ocrHandle);
    if (result == 0) {
        setError(QString("OCR分析失败，错误代码: %1").arg(result));
        return false;
    }
    
    qDebug() << "OCRDynamicLoader: OCR分析开始";
    return true;
}

bool OCRDynamicLoader::breakAnalyze()
{
    if (!m_ocrHandle) {
        return false;
    }
    
    if (!m_ocr_break_analyze) {
        qWarning() << "OCRDynamicLoader: breakAnalyze函数不可用";
        return false;
    }
    
    int result = m_ocr_break_analyze(m_ocrHandle);
    qDebug() << "OCRDynamicLoader: 中断分析，结果:" << result;
    return result == 0;
}

bool OCRDynamicLoader::isRunning() const
{
    if (!m_ocrHandle || !m_ocr_is_running) {
        return false;
    }
    
    return m_ocr_is_running(m_ocrHandle) != 0;
}

QString OCRDynamicLoader::getSimpleResult()
{
    if (!m_ocrHandle) {
        setError("OCR实例未创建");
        return QString();
    }
    
    if (!m_ocr_get_simple_result) {
        setError("ocr_get_simple_result函数未加载");
        return QString();
    }
    
    const char* result = m_ocr_get_simple_result(m_ocrHandle);
    if (!result) {
        setError("获取OCR结果失败");
        return QString();
    }
    
    QString text = QString::fromUtf8(result);
    qDebug() << "OCRDynamicLoader: 获取OCR结果成功，长度:" << text.length();
    return text;
}

TextBoxListC* OCRDynamicLoader::getTextBoxes()
{
    if (!m_ocrHandle) {
        setError("OCR实例未创建");
        return nullptr;
    }
    
    if (!m_ocr_get_text_boxes) {
        qWarning() << "OCRDynamicLoader: getTextBoxes函数不可用";
        return nullptr;
    }
    
    TextBoxListC* boxes = m_ocr_get_text_boxes(m_ocrHandle);
    if (boxes) {
        qDebug() << "OCRDynamicLoader: 获取文本框成功，数量:" << boxes->count;
    }
    
    return boxes;
}

QString OCRDynamicLoader::getVersion()
{
    if (!m_ocr_get_version) {
        return "版本信息不可用";
    }
    
    const char* version = m_ocr_get_version();
    return version ? QString::fromUtf8(version) : "未知版本";
}

void OCRDynamicLoader::setError(const QString& error)
{
    m_lastError = error;
    qWarning() << "OCRDynamicLoader错误:" << error;
} 