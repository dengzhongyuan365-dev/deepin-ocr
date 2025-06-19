// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <QImage>
#include <QString>

class OCRDynamicLoader;
class QSettings;

class OCREngine
{
public:
    OCREngine();
    ~OCREngine();

    bool isRunning() const
    {
        return m_isRunning;
    }

    bool setLanguage(const QString &language);
    void setImage(const QImage &image);
    QString getRecogitionResult();

private:
    OCRDynamicLoader *ocrLoader;
    std::atomic_bool m_isRunning;
    QSettings *ocrSetting;
};
