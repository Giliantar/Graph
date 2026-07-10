#include "graphwidget.h"

#include <QPainter>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QMouseEvent>
#include <QQueue>
#include <cmath>
#include <algorithm>
#include <limits>

GraphWidget::GraphWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);
    animTimer = new QTimer(this);
    connect(animTimer, &QTimer::timeout, this, &GraphWidget::animationStep);
}

void GraphWidget::setAutoLayout(bool enabled)
{
    autoLayout_ = enabled;
    if (enabled) {
        calculatePositions();
        updateEdgeScales();
        update();
    }
}

void GraphWidget::clearSelection()
{
    if (animRunning) stopCarAnimation();
    selectedStart = -1;
    selectedEnd = -1;
    pathVertices.clear();
    pathEdges.clear();
    pathTotalWeight = 0;
    update();
}

void GraphWidget::loadFromFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл");
        return;
    }

    QTextStream in(&file);
    int n, m;
    in >> n >> m;

    if (n <= 0) {
        QMessageBox::warning(this, "Ошибка", "Некорректное количество вершин");
        file.close();
        return;
    }

    QVector<QVector<int>> matrix(n, QVector<int>(n));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            in >> matrix[i][j];
        }
    }

    file.close();
    loadFromData(n, m, matrix);
}

void GraphWidget::updateEdgeScales()
{
    edgeScale = QVector<QVector<double>>(vertexCount_, QVector<double>(vertexCount_, 1.0));
    for (int i = 0; i < vertexCount_; ++i) {
        for (int j = i + 1; j < vertexCount_; ++j) {
            if (adjMatrix[i][j] != 0) {
                double len = edgeLength(i, j);
                edgeScale[i][j] = edgeScale[j][i] = (len > 0) ? adjMatrix[i][j] / len : 1.0;
            }
        }
    }
}

void GraphWidget::loadFromData(int vertexCount, int edgeCount, const QVector<QVector<int>> &matrix)
{
    vertexCount_ = vertexCount;
    edgeCount_ = edgeCount;
    adjMatrix = matrix;
    clearSelection();
    calculatePositions();
    updateEdgeScales();
    update();
}

void GraphWidget::calculatePositions()
{
    if (vertexCount_ == 0) return;

    positions.clear();
    positions.reserve(vertexCount_);

    double cx = width() / 2.0;
    double cy = height() / 2.0;
    double radius = std::min(cx, cy) * 0.55;

    for (int i = 0; i < vertexCount_; ++i) {
        double angle = 2.0 * M_PI * i / vertexCount_ - M_PI / 2.0;
        double x = cx + radius * std::cos(angle);
        double y = cy + radius * std::sin(angle);
        positions.append(QPointF(x, y));
    }
}

int GraphWidget::vertexAt(const QPoint &pos) const
{
    double vertexRadius = std::min(width(), height()) / (vertexCount_ * 3.5);
    vertexRadius = std::clamp(vertexRadius, 3.0, 14.0);

    for (int i = 0; i < positions.size(); ++i) {
        double dx = pos.x() - positions[i].x();
        double dy = pos.y() - positions[i].y();
        if (dx * dx + dy * dy <= vertexRadius * vertexRadius) {
            return i;
        }
    }
    return -1;
}

double GraphWidget::edgeLength(int i, int j) const
{
    double dx = positions[i].x() - positions[j].x();
    double dy = positions[i].y() - positions[j].y();
    return std::sqrt(dx * dx + dy * dy);
}

void GraphWidget::dijkstra(int start, int end)
{
    const int INF = std::numeric_limits<int>::max();
    QVector<int> dist(vertexCount_, INF);
    QVector<int> prev(vertexCount_, -1);
    QVector<bool> visited(vertexCount_, false);

    dist[start] = 0;

    for (int iter = 0; iter < vertexCount_; ++iter) {
        int u = -1;
        int best = INF;
        for (int v = 0; v < vertexCount_; ++v) {
            if (!visited[v] && dist[v] < best) {
                best = dist[v];
                u = v;
            }
        }
        if (u == -1 || u == end) break;
        visited[u] = true;

        for (int v = 0; v < vertexCount_; ++v) {
            if (adjMatrix[u][v] > 0 && !visited[v]) {
                int w = std::max(1, (int)std::round(edgeLength(u, v) * edgeScale[u][v]));
                int nd = dist[u] + w;
                if (nd < dist[v]) {
                    dist[v] = nd;
                    prev[v] = u;
                }
            }
        }
    }

    if (animRunning) stopCarAnimation();
    pathEdges.clear();
    pathVertices.clear();
    pathTotalWeight = 0;

    if (dist[end] == INF) {
        emit statusMessage("Путь не найден");
        return;
    }

    QVector<int> path;
    for (int v = end; v != -1; v = prev[v])
        path.prepend(v);

    pathVertices = path;
    pathTotalWeight = dist[end];

    for (int i = 0; i < path.size() - 1; ++i) {
        int a = path[i], b = path[i + 1];
        if (a > b) std::swap(a, b);
        pathEdges.insert({a, b});
    }

    emit pathFound(pathVertices, pathTotalWeight);
    emit statusMessage(QString("Кратчайший путь: %1 → %2, вес: %3")
                       .arg(start + 1).arg(end + 1).arg(dist[end]));
}

void GraphWidget::startCarAnimation()
{
    if (pathVertices.size() < 2) {
        emit statusMessage("Нет построенного пути для анимации");
        return;
    }
    animEdgeIndex = 0;
    animProgress = 0.0;
    animRunning = true;
    animTimer->start(20);
    emit statusMessage("Машина поехала...");
    update();
}

void GraphWidget::stopCarAnimation()
{
    animTimer->stop();
    animRunning = false;
    update();
}

void GraphWidget::animationStep()
{
    if (!animRunning || pathVertices.size() < 2) {
        stopCarAnimation();
        return;
    }

    animProgress += 0.02;

    if (animProgress >= 1.0) {
        animProgress = 0.0;
        animEdgeIndex++;
        if (animEdgeIndex >= pathVertices.size() - 1) {
            stopCarAnimation();
            emit statusMessage("Машина прибыла в пункт назначения!");
            emit animationFinished();
            return;
        }
    }
    update();
}

void GraphWidget::drawCar(QPainter &painter)
{
    if (animEdgeIndex >= pathVertices.size() - 1)
        return;

    double vertexRadius = std::min(width(), height()) / (vertexCount_ * 3.5);
    vertexRadius = std::clamp(vertexRadius, 3.0, 14.0);

    int from = pathVertices[animEdgeIndex];
    int to = pathVertices[animEdgeIndex + 1];

    QPointF p1 = positions[from];
    QPointF p2 = positions[to];

    QPointF carPos = p1 + (p2 - p1) * animProgress;

    double dx = p2.x() - p1.x();
    double dy = p2.y() - p1.y();
    double angle = atan2(dy, dx) * 180.0 / M_PI;

    double carLen = vertexRadius * 2.8;
    double carW = vertexRadius * 1.4;

    painter.save();

    painter.translate(carPos);
    painter.rotate(angle);

    QPointF bodyRect(-carLen / 2.0, -carW / 2.0);
    QRectF body(bodyRect, QSizeF(carLen, carW));

    QLinearGradient grad(bodyRect, QPointF(carLen / 2.0, -carW / 2.0));
    grad.setColorAt(0.0, QColor(180, 40, 40));
    grad.setColorAt(1.0, QColor(220, 60, 60));
    painter.setBrush(grad);
    painter.setPen(QPen(QColor(30, 30, 30), 1.2));
    painter.drawRoundedRect(body, carW * 0.25, carW * 0.25);

    double noseW = carW * 0.5;
    double noseH = carW * 0.45;
    QPolygonF nose;
    nose << QPointF(carLen / 2.0, 0.0)
         << QPointF(carLen / 2.0 - noseH, -noseW / 2.0)
         << QPointF(carLen / 2.0 - noseH, noseW / 2.0);
    painter.setBrush(QColor(255, 200, 50));
    painter.setPen(QPen(QColor(180, 130, 20), 1.0));
    painter.drawPolygon(nose);

    painter.restore();
}

void GraphWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::white);

    if (vertexCount_ == 0) return;

    double vertexRadius = std::min(width(), height()) / (vertexCount_ * 3.5);
    vertexRadius = std::clamp(vertexRadius, 3.0, 14.0);
    QFont weightFont = font();
    weightFont.setPointSize(std::max(7, std::min(11, 100 / vertexCount_)));

    int maxDisplay = 1;
    for (int i = 0; i < vertexCount_; ++i)
        for (int j = i + 1; j < vertexCount_; ++j)
            if (adjMatrix[i][j] != 0)
                maxDisplay = std::max(maxDisplay,
                    std::max(1, (int)std::round(edgeLength(i, j) * edgeScale[i][j])));

    for (int i = 0; i < vertexCount_; ++i) {
        for (int j = i + 1; j < vertexCount_; ++j) {
            if (adjMatrix[i][j] == 0) continue;

            int displayWeight = std::max(1, (int)std::round(edgeLength(i, j) * edgeScale[i][j]));

            QPair<int,int> edge(i, j);
            bool onPath = pathEdges.contains(edge);

            float thickness;
            QColor edgeColor;
            if (onPath) {
                thickness = 3.5f;
                edgeColor = QColor(20, 60, 140);
            } else {
                thickness = 0.5f + 2.5f * (float)displayWeight / (float)maxDisplay;
                thickness = std::min(thickness, 4.0f);
                edgeColor = QColor(40, 40, 40);
            }

            painter.setPen(QPen(edgeColor, thickness));
            painter.drawLine(positions[i], positions[j]);

            if (!onPath) {
                QPointF mid = (positions[i] + positions[j]) / 2.0;
                QString label = QString::number(displayWeight);
                QFontMetrics fm(weightFont);
                int tw = fm.horizontalAdvance(label) + 6;
                int th = fm.height();
                QRectF textRect(mid.x() - tw / 2.0, mid.y() - th / 2.0, tw, th);

                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(255, 255, 255, 180));
                painter.drawRect(textRect);

                painter.setFont(weightFont);
                painter.setPen(QColor(40, 40, 40));
                painter.drawText(textRect, Qt::AlignCenter, label);
            }
        }
    }

    QFont vertexFont = font();
    vertexFont.setPointSize(std::max(8, std::min(14, 100 / vertexCount_)));

    for (int i = 0; i < vertexCount_; ++i) {
        QPointF p = positions[i];
        QRectF rect(p.x() - vertexRadius, p.y() - vertexRadius,
                    2.0 * vertexRadius, 2.0 * vertexRadius);

        bool isStart = (i == selectedStart);
        bool isEnd = (i == selectedEnd);

        if (isStart || isEnd) {
            painter.setBrush(QColor(20, 60, 140));
            painter.setPen(QPen(QColor(20, 60, 140), 2));
        } else {
            painter.setBrush(Qt::white);
            painter.setPen(QPen(QColor(40, 40, 40), 1.5));
        }
        painter.drawEllipse(rect);

        painter.setFont(vertexFont);
        painter.setPen(isStart || isEnd ? Qt::white : QColor(40, 40, 40));
        painter.drawText(rect, Qt::AlignCenter, QString::number(i + 1));
    }

    if (selectedStart >= 0 && selectedEnd >= 0 && !pathVertices.isEmpty()) {
        QStringList pathParts;
        for (int v : pathVertices)
            pathParts << QString::number(v + 1);

        QString pathStr = pathParts.join(" → ");

        QStringList edgeParts;
        for (int i = 0; i < pathVertices.size() - 1; ++i) {
            int a = pathVertices[i], b = pathVertices[i + 1];
            int w = std::max(1, (int)std::round(edgeLength(a, b) * edgeScale[a][b]));
            edgeParts << QString("%1→%2(%3)").arg(a + 1).arg(b + 1).arg(w);
        }

        QString details = QString("Путь: %1\nРебер: %2\nВес: %3\n%4")
            .arg(pathStr)
            .arg(pathVertices.size() - 1)
            .arg(pathTotalWeight)
            .arg(edgeParts.join(" + "));

        QFont infoFont = font();
        infoFont.setPointSize(10);
        painter.setFont(infoFont);

        QFontMetrics fm(infoFont);
        int lineH = fm.height() + 2;
        int lines = details.count('\n') + 1;
        int boxW = 300;
        int boxH = lines * lineH + 12;
        int margin = 10;

        QRectF bgRect(margin, margin, boxW, boxH);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 220));
        painter.drawRoundedRect(bgRect, 6, 6);

        painter.setPen(QColor(20, 60, 140));
        QRectF textRect(margin + 6, margin + 6, boxW - 12, boxH - 12);
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignTop, details);
    }

    if (animRunning) {
        drawCar(painter);
    }
}

void GraphWidget::resizeEvent(QResizeEvent *)
{
    if (autoLayout_)
        calculatePositions();
    update();
}

void GraphWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    pressVertex = vertexAt(event->pos());
    pressPos = event->pos();
}

void GraphWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (pressVertex >= 0 && draggedVertex < 0) {
        QPoint delta = event->pos() - pressPos;
        if (delta.manhattanLength() > 5) {
            draggedVertex = pressVertex;
            dragOffset = positions[pressVertex] - QPointF(pressPos);
            autoLayout_ = false;
            emit vertexDragStarted(draggedVertex);
            setCursor(Qt::ClosedHandCursor);
        }
    }

    if (draggedVertex >= 0) {
        positions[draggedVertex] = QPointF(event->pos()) + dragOffset;
        emit vertexMoved(draggedVertex, positions[draggedVertex]);
        if (selectedStart >= 0 && selectedEnd >= 0)
            dijkstra(selectedStart, selectedEnd);
        update();
        return;
    }

    int idx = vertexAt(event->pos());
    if (idx != hoveredVertex) {
        hoveredVertex = idx;
        setCursor(idx >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
        update();
        if (idx >= 0)
            emit vertexHovered(idx);
    }
}

void GraphWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    if (draggedVertex >= 0) {
        int idx = draggedVertex;
        draggedVertex = -1;
        pressVertex = -1;
        setCursor(Qt::ArrowCursor);
        emit vertexDragFinished(idx);
        return;
    }

    int idx = pressVertex;
    pressVertex = -1;
    if (idx < 0 || idx != vertexAt(event->pos())) return;

    emit vertexClicked(idx);

    if (selectedStart < 0) {
        selectedStart = idx;
        emit statusMessage(QString("Старт: %1. Выберите конечную вершину").arg(idx + 1));
    } else if (selectedEnd < 0) {
        if (idx == selectedStart) {
            clearSelection();
            emit statusMessage("Выбор сброшен");
            return;
        }
        selectedEnd = idx;
        dijkstra(selectedStart, selectedEnd);
    } else {
        clearSelection();
        selectedStart = idx;
        emit statusMessage(QString("Старт: %1. Выберите конечную вершину").arg(idx + 1));
    }
    update();
}
