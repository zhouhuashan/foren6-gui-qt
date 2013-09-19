#include "rplNode.h"
#include <QPointF>
#include <QBrush>
#include "rplLink.h"
#include <QGraphicsSceneMouseEvent>
#include "rplNetworkInfoManager.h"
#include <QApplication>
#include <QMenu>
#include <QInputDialog>

namespace rpl
{

static qreal const defaultNodeSize = 32;

Node::Node(NetworkInfoManager *networkInfoManager, di_node_t *nodeData, int version)
	: _networkInfoManager(networkInfoManager),
	  _nodeData(0),
	  _ellipse(this),
	  _label(this),
      _infoLabel(this),
	  _dx(0),
	  _dy(0),
	  _isBeingMoved(false),
	  _pinned(false),
	  _isSelected(false)
{
	setFlags( QGraphicsItem::ItemIsFocusable | QGraphicsItem::ItemIsMovable);
	setAcceptHoverEvents( true );
	setNodeData(nodeData, version);

    QSettings settings;
    _maxSize = settings.value("node_size", defaultNodeSize).toFloat();
    qDebug("Node: %f", _maxSize);
	_ellipse.setRect(0, 0, _maxSize, _maxSize);
	this->addToGroup(&_ellipse);

    QFont newFont = QApplication::font();
    newFont.setPointSize(8);
    _label.setFont(newFont);
    this->addToGroup(&_label);
    setName("");

    _infoLabel.setFont(newFont);
    this->addToGroup(&_infoLabel);
    setInfoText("");

    setCenterPos(qrand()%500, qrand()%500);
	setZValue(1);

	qstrcpy(guard, "node");
}

Node::~Node() {
	Link *link;

	foreach(link, _links) {
		delete link;
	}

	_links.clear();

	qstrcpy(guard, "edon");
	setNodeData(0, -21);
}

void Node::setName(QString const & name ) {
    _friendlyName = name;
    if (! name.isEmpty()) {
        _label.setText(name);
    } else {
        _label.setText(QString::number((node_get_key(_nodeData)->ref.wpan_address & 0xFF), 16));
    }
    qreal y= _maxSize/2 - _label.boundingRect().height();
    _label.setPos(_maxSize/2 - _label.boundingRect().width()/2, y );
}

void Node::setInfoText(QString infoText) {
    _infoLabel.setText(infoText);
    _infoLabel.setPos(_maxSize/2 - _infoLabel.boundingRect().width()/2, _maxSize/2);
}

void Node::setCenterPos(QPointF newpos) {
	setCenterPos(newpos.x(), newpos.y());
}

void Node::setCenterPos(qreal x, qreal y) {
    setPos(x - _maxSize/2, y - _maxSize/2);
}

void Node::setPos(qreal x, qreal y) {
	Link *link;

	QGraphicsItemGroup::setPos(x, y);

	foreach(link, _links) {
		link->updatePosition();
	}
}

QPointF Node::centerPos() const {
    return QPointF(x() + _maxSize/2, y() + _maxSize/2);
}

void Node::incSpeed(qreal x, qreal y) {
	if(_isBeingMoved == false && _pinned == false) {
		_dx += x;
		_dy += y;
	}
}

void Node::updatePosition() {
	if(_isBeingMoved == false && _pinned == false) {
		qint64 interval = 40;
		qreal newX = centerX() + _dx*interval/1000;
		qreal newY = centerY() + _dy*interval/1000;
		_dx *= 0.9;
		_dy *= 0.9;

		if(newX < 0) {
			newX = 0;
			_dx = -_dx/2;
		}
		if(newY < 0) {
			newY = 0;
			_dy = - _dy/2;
		}
		if(newX > 500) {
			newX = 500;
			_dx = -_dx/2;
		}
		if(newY > 500) {
			newY = 500;
			_dy = - _dy/2;
		}
		setCenterPos(newX, newY);
	}
}

void Node::contextMenuEvent( QGraphicsSceneContextMenuEvent * event ) {
    QMenu menu;

    QAction *lockAction = menu.addAction(_pinned ? "Unlock" : "Lock");
    QAction *renameAction = menu.addAction("Rename...");
    QAction *selectedAction = menu.exec(event->screenPos());
    if ( selectedAction == lockAction ) {
      _pinned = !_pinned;
    } else if ( selectedAction == renameAction ) {
        bool ok;
        QString name = QInputDialog::getText(0,
            "Set node name",
             "Friendly node name :",
             QLineEdit::Normal,
             _friendlyName,
             &ok);
        if (ok) {
           setName(name);
        }
    }
}

void Node::mousePressEvent(QGraphicsSceneMouseEvent *event) {
	QGraphicsItemGroup::mousePressEvent(event);
	if(event->button() == Qt::LeftButton) {
		_timeElapsedMouseMove.invalidate();
		_isBeingMoved = true;
	} else if(event->button() == Qt::MiddleButton) {
		_pinned = !_pinned;
	}
}

void Node::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
	QGraphicsItemGroup::mouseReleaseEvent(event);
	if(event->button() == Qt::LeftButton) {
		if((event->buttonDownScenePos(Qt::LeftButton) - event->scenePos()).manhattanLength() < 4) {
			_networkInfoManager->selectNode(this);
		} else {
			if(_timeElapsedMouseMove.isValid()) {
				qint64 elapsedTime = _timeElapsedMouseMove.restart();
				if(elapsedTime) {
					_dx = (event->scenePos().x() - event->lastScenePos().x()) * 1000 / elapsedTime;
					_dy = (event->scenePos().y() - event->lastScenePos().y()) * 1000 / elapsedTime;
				}
			}
		}

		_isBeingMoved = false;
	}
}

void Node::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
	if(_isBeingMoved) {
		Link *link;
		if(_timeElapsedMouseMove.isValid() == false)
			_timeElapsedMouseMove.start();
		else {
			qint64 elapsedTime = _timeElapsedMouseMove.restart();
			if(elapsedTime && !_pinned) {
				_dx = (event->scenePos().x() - event->lastScenePos().x()) * 1000 / elapsedTime;
				_dy = (event->scenePos().y() - event->lastScenePos().y()) * 1000 / elapsedTime;
			}
		}

		QGraphicsItemGroup::mouseMoveEvent(event);

		foreach(link, _links) {
			link->updatePosition();
		}
	}
}

void Node::setNodeData(di_node_t *data, int version) {
	_nodeData = data;
	_version = version;
}

}
