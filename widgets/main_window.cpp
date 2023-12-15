#include "main_window.h"

#include <QtGui/QAction>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>

#include "main_window_actions.h"

const Qt::WindowFlags initialWindowFlags = Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint;
const QSize buttonSize(48, 48);
const int expandedWidth = 384;

class DragWidget : public QWidget
{
    Q_DISABLE_COPY_MOVE(DragWidget);
    QPoint m_lastMousePos;

public:
    explicit DragWidget(MainWindow *parent) : QWidget(parent)
    {
        setCursor(Qt::SizeAllCursor);
    };
    ~DragWidget() override = default;

    inline QSize sizeHint() const override { return {buttonSize.width() / 2, buttonSize.height()}; }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            m_lastMousePos = event->globalPos();
        }
    };

    void mouseMoveEvent(QMouseEvent *event) override
    {
        // TODO Use window()->windowHandle()->startSystemMove()
        if (parentWidget() && event->buttons() & Qt::LeftButton)
        {
            const QPoint diff = event->globalPos() - m_lastMousePos;
            m_lastMousePos = event->globalPos();

            parentWidget()->move(parentWidget()->pos() + diff);

            event->accept();
        }
    }

    void paintEvent([[maybe_unused]] QPaintEvent *event) override
    {
        const QSize margin(8, 8);
        const QColor dotColor(196, 196, 196);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QPen pen(Qt::DotLine);
        pen.setColor(dotColor);
        pen.setWidth(2);
        painter.setPen(pen);

        // The start and end points of the first dotted line
        const QPoint lineStart(margin.width(), margin.height());
        const QPoint lineEnd(lineStart.x(), 48 - margin.height());

        painter.drawLine(lineStart, lineEnd);
        painter.drawLine(lineStart.x() + 4, lineStart.y(), lineStart.x() + 4, lineEnd.y());
    }
};

QToolButton *createToolButton(MainWindow *parent, QAction &action)
{
    auto *button = new QToolButton(parent);
    button->setDefaultAction(&action);
    button->setFixedSize(buttonSize);
    parent->addWidget(button);
    return button;
}

MainWindow::MainWindow(QWidget *parent)
    : QToolBar(parent), m_dragWidget(new DragWidget(this))
{
    m_windowActions.reset(new Actions(this));

    setWindowFlags(initialWindowFlags);

    addWidget(m_dragWidget);
    addSeparator();

    addAction(&m_windowActions->toggleGhostMode());
    // createToolButton(this, m_windowActions->toggleGhostMode());
    addSeparator();

    createToolButton(this, m_windowActions->open());
    createToolButton(this, m_windowActions->save());
    addSeparator();

    createToolButton(this, m_windowActions->minimizeApplication());
    createToolButton(this, m_windowActions->closeApplication());
}

QSize MainWindow::sizeHint() const
{
    return {expandedWidth, buttonSize.height()};
}
