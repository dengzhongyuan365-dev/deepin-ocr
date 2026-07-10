// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "OcrResult.h"

#include <DOcr>
#include <docr.h>

#include <QtMath>
#include <QLineF>
#include <QVector>
#include <algorithm>
#include <limits>

namespace {

QRectF boxRect(const Dtk::Ocr::TextBox &box)
{
    if (box.points.size() < 4) {
        return {};
    }

    qreal minX = box.points.first().x();
    qreal maxX = minX;
    qreal minY = box.points.first().y();
    qreal maxY = minY;
    for (const auto &point : box.points) {
        minX = qMin(minX, point.x());
        maxX = qMax(maxX, point.x());
        minY = qMin(minY, point.y());
        maxY = qMax(maxY, point.y());
    }
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

qreal centerY(const QRectF &rect)
{
    return rect.center().y();
}

qreal centerX(const QRectF &rect)
{
    return rect.center().x();
}

qreal characterDisplayWeight(QChar ch)
{
    const ushort unicode = ch.unicode();
    if (unicode <= 0x007F) {
        return 1.0;
    }
    if (unicode >= 0xFF01 && unicode <= 0xFF60) {
        return 1.0;
    }
    return 2.0;
}

QList<OcrCharItem> buildCharItems(const OcrLineItem &line, int lineIndex, const QList<Dtk::Ocr::TextBox> &charBoxes)
{
    QList<OcrCharItem> chars;
    if (line.text.isEmpty()) {
        return chars;
    }

    const QRectF lineRect = OcrResult::textBoxRect(line.box);
    const int textLen = line.text.size();
    chars.reserve(textLen);

    QVector<qreal> weights(textLen);
    qreal totalWeight = 0.0;
    for (int charIndex = 0; charIndex < textLen; ++charIndex) {
        if (charBoxes.size() == textLen) {
            weights[charIndex] = qMax(OcrResult::textBoxRect(charBoxes.at(charIndex)).width(), 0.1);
        } else {
            weights[charIndex] = characterDisplayWeight(line.text.at(charIndex));
        }
        totalWeight += weights[charIndex];
    }
    if (totalWeight <= 0.0) {
        totalWeight = textLen;
        weights.fill(1.0, textLen);
    }

    qreal x = lineRect.left();
    for (int charIndex = 0; charIndex < textLen; ++charIndex) {
        const qreal charWidth = lineRect.width() * weights.at(charIndex) / totalWeight;
        OcrCharItem item;
        Dtk::Ocr::TextBox box;
        box.angle = 0;
        box.points.push_back(QPointF(x, lineRect.top()));
        box.points.push_back(QPointF(x + charWidth, lineRect.top()));
        box.points.push_back(QPointF(x + charWidth, lineRect.bottom()));
        box.points.push_back(QPointF(x, lineRect.bottom()));
        item.box = box;
        item.lineIndex = lineIndex;
        item.charIndexInLine = charIndex;
        item.character = line.text.at(charIndex);
        chars.append(item);
        x += charWidth;
    }
    return chars;
}

QList<QList<int>> groupIntoVisualLines(QList<OcrLineItem> &rawLines)
{
    QList<int> order;
    order.reserve(rawLines.size());
    for (int i = 0; i < rawLines.size(); ++i) {
        order.append(i);
    }

    std::sort(order.begin(), order.end(), [&rawLines](int a, int b) {
        const QRectF rectA = OcrResult::textBoxRect(rawLines[a].box);
        const QRectF rectB = OcrResult::textBoxRect(rawLines[b].box);
        const qreal ya = centerY(rectA);
        const qreal yb = centerY(rectB);
        if (qAbs(ya - yb) > 4.0) {
            return ya < yb;
        }
        return centerX(rectA) < centerX(rectB);
    });

    QList<QList<int>> groups;
    for (int index : order) {
        const QRectF rect = OcrResult::textBoxRect(rawLines[index].box);
        const qreal cy = centerY(rect);
        const qreal height = rect.height();

        int targetGroup = -1;
        for (int groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
            const int rep = groups[groupIndex].constFirst();
            const QRectF repRect = OcrResult::textBoxRect(rawLines[rep].box);
            if (qAbs(cy - centerY(repRect)) < qMin(height, repRect.height()) * 0.55) {
                targetGroup = groupIndex;
                break;
            }
        }

        if (targetGroup < 0) {
            groups.append(QList<int>{index});
        } else {
            groups[targetGroup].append(index);
        }
    }

    for (auto &group : groups) {
        std::sort(group.begin(), group.end(), [&rawLines](int a, int b) {
            return centerX(OcrResult::textBoxRect(rawLines[a].box))
                < centerX(OcrResult::textBoxRect(rawLines[b].box));
        });
    }

    std::sort(groups.begin(), groups.end(), [&rawLines](const QList<int> &a, const QList<int> &b) {
        return centerY(OcrResult::textBoxRect(rawLines[a.constFirst()].box))
            < centerY(OcrResult::textBoxRect(rawLines[b.constFirst()].box));
    });

    return groups;
}

bool needSpaceBetweenBoxes(const OcrLineItem &previous, const OcrLineItem &current)
{
    const QRectF prevRect = OcrResult::textBoxRect(previous.box);
    const QRectF currRect = OcrResult::textBoxRect(current.box);
    const qreal gap = currRect.left() - prevRect.right();
    const qreal threshold = qMin(prevRect.height(), currRect.height()) * 0.35;
    return gap > threshold;
}

} // namespace

QRectF OcrResult::textBoxRect(const Dtk::Ocr::TextBox &box)
{
    return boxRect(box);
}

bool OcrResult::textBoxContains(const Dtk::Ocr::TextBox &box, const QPointF &point)
{
    return boxRect(box).contains(point);
}

OcrResult OcrResult::fromDOcr(Dtk::Ocr::DOcr *ocr)
{
    OcrResult result;
    if (ocr == nullptr) {
        return result;
    }

    const auto textBoxes = ocr->textBoxes();
    if (textBoxes.isEmpty()) {
        return result;
    }

    QList<OcrLineItem> rawLines;
    rawLines.reserve(textBoxes.size());
    for (int i = 0; i < textBoxes.size(); ++i) {
        OcrLineItem line;
        line.box = textBoxes.at(i);
        line.text = ocr->resultFromBox(i);
        if (line.text.isEmpty()) {
            continue;
        }
        line.chars = buildCharItems(line, rawLines.size(), ocr->charBoxes(i));
        rawLines.append(line);
    }

    const auto visualLines = groupIntoVisualLines(rawLines);
    int offset = 0;
    for (int visualIndex = 0; visualIndex < visualLines.size(); ++visualIndex) {
        const auto &group = visualLines.at(visualIndex);
        if (visualIndex > 0) {
            result.plainText.append('\n');
            ++offset;
        }

        for (int boxIndex = 0; boxIndex < group.size(); ++boxIndex) {
            const int rawIndex = group.at(boxIndex);
            auto &line = rawLines[rawIndex];

            if (boxIndex > 0) {
                const int prevRawIndex = group.at(boxIndex - 1);
                if (needSpaceBetweenBoxes(rawLines[prevRawIndex], line)) {
                    result.plainText.append(' ');
                    ++offset;
                }
            }

            line.plainTextStart = offset;
            result.plainText.append(line.text);
            offset += line.text.size();
            line.plainTextEnd = offset;

            for (auto &item : line.chars) {
                item.plainTextStart = line.plainTextStart + item.charIndexInLine;
                item.plainTextEnd = item.plainTextStart + 1;
                item.lineIndex = result.lines.size();
                result.allChars.append(item);
            }
            result.lines.append(line);
        }
    }

    return result;
}

int OcrResult::charIndexAt(const QPointF &imagePos) const
{
    for (int i = 0; i < allChars.size(); ++i) {
        if (textBoxContains(allChars.at(i).box, imagePos)) {
            return i;
        }
    }

    int bestLine = -1;
    qreal bestYDistance = std::numeric_limits<qreal>::max();
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QRectF rect = textBoxRect(lines.at(lineIndex).box);
        if (rect.contains(imagePos)) {
            bestLine = lineIndex;
            break;
        }
        const qreal yDistance = qAbs(imagePos.y() - rect.center().y());
        if (yDistance <= rect.height() * 0.55 && yDistance < bestYDistance) {
            bestYDistance = yDistance;
            bestLine = lineIndex;
        }
    }

    if (bestLine < 0) {
        return -1;
    }

    const auto &line = lines.at(bestLine);
    if (line.chars.isEmpty()) {
        return -1;
    }

    int bestGlobalChar = -1;
    qreal bestXDistance = std::numeric_limits<qreal>::max();
    for (int i = 0; i < allChars.size(); ++i) {
        if (allChars.at(i).lineIndex != bestLine) {
            continue;
        }
        const QRectF charRect = textBoxRect(allChars.at(i).box);
        if (charRect.contains(imagePos)) {
            return i;
        }
        if (imagePos.x() >= charRect.left() && imagePos.x() <= charRect.right()) {
            return i;
        }
        const qreal xDistance = qAbs(imagePos.x() - charRect.center().x());
        if (xDistance < bestXDistance) {
            bestXDistance = xDistance;
            bestGlobalChar = i;
        }
    }
    return bestGlobalChar;
}

QList<QPair<int, int>> OcrResult::plainTextRangesForRect(const QRectF &rect) const
{
    if (rect.isNull() || rect.isEmpty()) {
        return {};
    }

    QList<int> hitIndices;
    hitIndices.reserve(allChars.size());
    for (int i = 0; i < allChars.size(); ++i) {
        const QRectF charRect = textBoxRect(allChars.at(i).box);
        if (rect.intersects(charRect)) {
            hitIndices.append(i);
        }
    }

    if (hitIndices.isEmpty()) {
        return {};
    }

    std::sort(hitIndices.begin(), hitIndices.end());

    QList<QPair<int, int>> ranges;
    int rangeStart = allChars.at(hitIndices.first()).plainTextStart;
    int rangeEnd = allChars.at(hitIndices.first()).plainTextEnd;
    int prevCharIndex = hitIndices.first();

    for (int k = 1; k < hitIndices.size(); ++k) {
        const int idx = hitIndices.at(k);
        const auto &item = allChars.at(idx);
        if (idx == prevCharIndex + 1) {
            rangeEnd = item.plainTextEnd;
        } else {
            ranges.append(qMakePair(rangeStart, rangeEnd));
            rangeStart = item.plainTextStart;
            rangeEnd = item.plainTextEnd;
        }
        prevCharIndex = idx;
    }
    ranges.append(qMakePair(rangeStart, rangeEnd));
    return ranges;
}

QString OcrResult::textInRanges(const QList<QPair<int, int>> &ranges) const
{
    QString result;
    for (const auto &range : ranges) {
        result.append(textInRange(range.first, range.second));
    }
    return result;
}

QString OcrResult::textInRange(int start, int end) const
{
    if (start < 0 || end <= start || start >= plainText.size()) {
        return {};
    }
    return plainText.mid(start, qMin(end, plainText.size()) - start);
}

QList<int> OcrResult::charIndicesInRange(int start, int end) const
{
    QList<int> indices;
    for (int i = 0; i < allChars.size(); ++i) {
        const auto &item = allChars.at(i);
        if (item.plainTextEnd <= start || item.plainTextStart >= end) {
            continue;
        }
        indices.append(i);
    }
    return indices;
}

QList<QRectF> OcrResult::highlightRects(int start, int end) const
{
    QList<QRectF> rects;
    if (start < 0 || end <= start) {
        return rects;
    }

    for (const auto &line : lines) {
        if (line.plainTextEnd <= start || line.plainTextStart >= end) {
            continue;
        }

        QRectF united;
        for (const auto &item : line.chars) {
            if (item.plainTextEnd <= start || item.plainTextStart >= end) {
                continue;
            }
            const QRectF charRect = textBoxRect(item.box);
            united = united.isNull() ? charRect : united.united(charRect);
        }

        if (united.isNull()) {
            united = textBoxRect(line.box);
        }
        if (!united.isNull()) {
            rects.append(united);
        }
    }
    return rects;
}

QList<QRectF> OcrResult::highlightRectsForRanges(const QList<QPair<int, int>> &ranges) const
{
    QList<QRectF> rects;
    for (const auto &range : ranges) {
        rects.append(highlightRects(range.first, range.second));
    }
    return rects;
}
