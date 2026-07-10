#ifndef GRAPHWIDGET_H
#define GRAPHWIDGET_H

#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QSet>
#include <QTimer>

class GraphWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GraphWidget(QWidget *parent = nullptr);

    void loadFromFile(const QString &filename);
    void loadFromData(int vertexCount, int edgeCount, const QVector<QVector<int>> &adjMatrix);

    int vertexAt(const QPoint &pos) const;
    QVector<QVector<int>> adjacencyMatrix() const { return adjMatrix; }
    int vertexCount() const { return vertexCount_; }
    int edgeCount() const { return edgeCount_; }
    void setAutoLayout(bool enabled);
    void clearSelection();

signals:
    void vertexClicked(int index);
    void vertexHovered(int index);
    void vertexMoved(int index, QPointF pos);
    void vertexDragStarted(int index);
    void vertexDragFinished(int index);
    void pathFound(const QVector<int> &path, int totalWeight);
    void statusMessage(const QString &msg);
    void animationFinished();

public slots:
    void startCarAnimation();
    void stopCarAnimation();

public:
    bool isAnimRunning() const { return animRunning; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void calculatePositions();
    void updateEdgeScales();
    double edgeLength(int i, int j) const;
    void dijkstra(int start, int end);

    int vertexCount_ = 0;
    int edgeCount_ = 0;
    QVector<QVector<int>> adjMatrix;
    QVector<QVector<double>> edgeScale;
    QVector<QPointF> positions;
    int hoveredVertex = -1;
    int draggedVertex = -1;
    QPointF dragOffset;
    bool autoLayout_ = true;

    int selectedStart = -1;
    int selectedEnd = -1;
    QVector<int> pathVertices;
    QSet<QPair<int,int>> pathEdges;
    int pathTotalWeight = 0;

    int pressVertex = -1;
    QPoint pressPos;

    void drawCar(QPainter &painter);
    void animationStep();

    QTimer *animTimer = nullptr;
    bool animRunning = false;
    int animEdgeIndex = 0;
    double animProgress = 0.0;
};

inline uint qHash(const QPair<int,int> &p, uint seed = 0)
{
    return qHash(p.first, seed) ^ qHash(p.second, seed);
}

#endif // GRAPHWIDGET_H
