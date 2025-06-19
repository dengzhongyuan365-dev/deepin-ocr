// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QLibrary>
#include <QImage>
#include <QString>
#include <QStringList>
#include <memory>

// 从dtkocr库中导入的C API类型定义
typedef void* OCRHandle;

typedef enum {
    HARDWARE_CPU_ANY = 0,
    HARDWARE_GPU_VULKAN = 101
} HardwareType;

typedef struct {
    float points[8]; // 4个点的x,y坐标
    float angle;
} TextBoxC;

typedef struct {
    TextBoxC* boxes;
    int count;
} TextBoxListC;

// 函数指针类型定义
typedef OCRHandle (*ocr_create_func)();
typedef void (*ocr_destroy_func)(OCRHandle handle);
typedef int (*ocr_load_default_plugin_func)(OCRHandle handle);
typedef int (*ocr_plugin_ready_func)(OCRHandle handle);
typedef int (*ocr_set_hardware_func)(OCRHandle handle, HardwareType type, int device_id);
typedef int (*ocr_set_max_threads_func)(OCRHandle handle, int count);
typedef int (*ocr_set_language_func)(OCRHandle handle, const char* language);
typedef int (*ocr_set_image_file_func)(OCRHandle handle, const char* file_path);
typedef int (*ocr_set_image_data_func)(OCRHandle handle, const unsigned char* data, 
                                      int width, int height, int channels);
typedef int (*ocr_analyze_func)(OCRHandle handle);
typedef int (*ocr_break_analyze_func)(OCRHandle handle);
typedef int (*ocr_is_running_func)(OCRHandle handle);
typedef const char* (*ocr_get_simple_result_func)(OCRHandle handle);
typedef TextBoxListC* (*ocr_get_text_boxes_func)(OCRHandle handle);
typedef void (*ocr_free_text_boxes_func)(TextBoxListC* boxes);
typedef const char* (*ocr_get_version_func)();
// 新增：API版本查询函数指针
typedef int (*ocr_get_api_version_major_func)();
typedef int (*ocr_get_api_version_minor_func)();
typedef int (*ocr_get_api_version_patch_func)();

/**
 * @brief OCR动态库加载器
 * 
 * 这个类负责动态加载dtk6ocr库并提供OCR功能接口
 * 兼容Qt5和Qt6，支持符号版本检查和ABI兼容性验证
 */
class OCRDynamicLoader
{
public:
    OCRDynamicLoader();
    ~OCRDynamicLoader();
    
    bool loadLibrary();
    void unloadLibrary();
    bool isLoaded() const;
    
    // 版本兼容性检查
    bool isAPICompatible() const;
    QString getAPIVersionInfo() const;
    
    // OCR操作接口
    bool createOCR();
    void destroyOCR();
    bool loadDefaultPlugin();
    bool pluginReady() const;
    
    bool setHardware(HardwareType type, int deviceId = 0);
    bool setMaxThreads(int count);
    bool setLanguage(const QString& language);
    bool setImageFile(const QString& filePath);
    bool setImage(const QImage& image);
    
    bool analyze();
    bool breakAnalyze();
    bool isRunning() const;
    
    QString getSimpleResult();
    TextBoxListC* getTextBoxes();
    
    QString getVersion();
    QString getLastError() const { return m_lastError; }

private:
    // 库搜索和加载相关方法
    QStringList getSystemLibraryPaths();
    QStringList getLibraryNames();
    QString findLibraryFile();
    bool loadFunctions();
    bool loadVersionFunctions();
    void setError(const QString& error);
    
    std::unique_ptr<QLibrary> m_library;
    OCRHandle m_ocrHandle;
    QString m_lastError;
    
    // 函数指针
    ocr_create_func m_ocr_create;
    ocr_destroy_func m_ocr_destroy;
    ocr_load_default_plugin_func m_ocr_load_default_plugin;
    ocr_plugin_ready_func m_ocr_plugin_ready;
    ocr_set_hardware_func m_ocr_set_hardware;
    ocr_set_max_threads_func m_ocr_set_max_threads;
    ocr_set_language_func m_ocr_set_language;
    ocr_set_image_file_func m_ocr_set_image_file;
    ocr_set_image_data_func m_ocr_set_image_data;
    ocr_analyze_func m_ocr_analyze;
    ocr_break_analyze_func m_ocr_break_analyze;
    ocr_is_running_func m_ocr_is_running;
    ocr_get_simple_result_func m_ocr_get_simple_result;
    ocr_get_text_boxes_func m_ocr_get_text_boxes;
    ocr_free_text_boxes_func m_ocr_free_text_boxes;
    ocr_get_version_func m_ocr_get_version;
    
    // 新增：API版本查询函数指针
    ocr_get_api_version_major_func m_ocr_get_api_version_major;
    ocr_get_api_version_minor_func m_ocr_get_api_version_minor;
    ocr_get_api_version_patch_func m_ocr_get_api_version_patch;
}; 