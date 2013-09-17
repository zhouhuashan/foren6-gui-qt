#include "rplTreeScene.h"
#include "rplNode.h"
#include <rpl_packet_parser.h>
#include <QGraphicsTextItem>
#include <uthash.h>
#include <utlist.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "rplLink.h"
#include <math.h>

#include <QVector2D>
#include <QPainter>

namespace rpl
{

TreeScene::TreeScene()
{
	_updateDagsTimer.setInterval(40);
	_updateDagsTimer.setSingleShot(false);
	connect(&_updateDagsTimer, SIGNAL(timeout()), this, SLOT(updateNodePositions()));
	_updateDagsTimer.start();
	layout = 0;
	background = new QPixmap();
}

void TreeScene::setLayout(QSettings *  newLayout) {
  layout = newLayout;
  delete background;
  if ( layout->contains("background") ) {
    backgroundFile = layout->value("background", "").toString();
    background = new QPixmap(backgroundFile);
  } else {
    background = new QPixmap();
  }
  setSceneRect(background->rect());
  Node * node;
  foreach (node, _nodes) {
    layout->beginGroup(QString::number(node_get_key(node->getNodeData())->ref.wpan_address, 16));
    if ( layout->contains("x") ) {
        node->setPos( layout->value("x", 0).toFloat(), layout->value("y", 0).toFloat());
    }
    node->setLocked(layout->value("locked", true).toBool());
    if ( layout->contains("name") ) {
        node->setName(layout->value("name", "").toString());
    } else {
      node->setDefaultName();
    }
    layout->endGroup();
  }
}

void TreeScene::getLayout(QSettings *  newLayout) {
  layout = newLayout;
  layout->setValue("background", backgroundFile);
  Node * node;
  foreach (node, _nodes) {
    layout->beginGroup(QString::number(node_get_key(node->getNodeData())->ref.wpan_address, 16));
    layout->setValue("x", node->x());
    layout->setValue("y", node->y());
    layout->setValue("locked", node->isLocked());
    layout->endGroup();
  }
}

void TreeScene::clearLayout() {
  delete background;
  background = new QPixmap();
  setSceneRect(background->rect());
  Node * node;
  foreach (node, _nodes) {
    node->setLocked(false);
    node->setDefaultName();
  }
  layout = 0;
}

void TreeScene::drawBackground( QPainter * painter, const QRectF & rect ) {
	painter->save();
	painter->drawPixmap(rect, *background, rect);
	painter->restore();
}

void TreeScene::toggleNodeMovement() {
	if(_updateDagsTimer.isActive())
		_updateDagsTimer.stop();
	else _updateDagsTimer.start();
}

void TreeScene::addNode(Node *node) {
	//qDebug("Adding Node %p %llX", node, node_get_mac64(node->getNodeData()));
	addItem(node);
	_nodes.insert(node_get_key(node->getNodeData())->ref.wpan_address, node);
	if ( layout ) {
	  layout->beginGroup(QString::number(node_get_key(node->getNodeData())->ref.wpan_address, 16));
      if ( layout->contains("x") ) {
          node->setPos( layout->value("x", 0).toFloat(), layout->value("y", 0).toFloat());
      }
      node->setLocked(layout->value("locked", false).toBool());
      if ( layout->contains("name") ) {
          node->setName(layout->value("name", "").toString());
      }
      layout->endGroup();
	}
}

void TreeScene::addLink(Link *link) {
	//qDebug("Adding Link %p %llX -> %llX", link, link->getLinkData()->key.ref.child.wpan_address, link->getLinkData()->key.ref.parent.wpan_address);
	addItem(link);

	QPair<addr_wpan_t, addr_wpan_t> linkKey;
	linkKey.first = link_get_key(link->getLinkData())->ref.child.wpan_address;
	linkKey.second = link_get_key(link->getLinkData())->ref.parent.wpan_address;
	_links.insert(linkKey, link);
}

void TreeScene::removeNode(Node *node) {
	//qDebug("Removing Node %p", node);
	removeItem(node);
	_nodes.remove(node_get_key(node->getNodeData())->ref.wpan_address);
}

void TreeScene::removeAllNodes() {
	Node *node;
	foreach(node, _nodes) {
		delete node;
	}
	_nodes.clear();
}

void TreeScene::removeLink(Link *link) {
	//qDebug("Removing Link %p, raw link %p", link, link->getLinkData());
	removeItem(link);

	QPair<addr_wpan_t, addr_wpan_t> linkKey;
	linkKey.first = link_get_key(link->getLinkData())->ref.child.wpan_address;
	linkKey.second = link_get_key(link->getLinkData())->ref.parent.wpan_address;
	_links.remove(linkKey);
}

void TreeScene::removeAllLinks() {
	Link *link;
	foreach(link, _links) {
		delete link;
	}
	_links.clear();
}

void TreeScene::clear() {
	removeAllLinks();
	removeAllNodes();
	QGraphicsScene::clear();
}

Node *TreeScene::getNode(addr_wpan_t address) {
	return _nodes.value(address, 0);
}

Link *TreeScene::getLink(addr_wpan_t child, addr_wpan_t parent) {
	QPair<addr_wpan_t, addr_wpan_t> linkKey(child, parent);

	return _links.value(linkKey, 0);
}

void TreeScene::updateNodePositions() {
	Link *currentLink;
	Node *n1, *n2;

	foreach(n1, _nodes) {
		foreach(currentLink, n1->links()) {
			QPointF pos1 = currentLink->from()->centerPos();
			QPointF pos2 = currentLink->to()->centerPos();
			qreal vx = pos2.x() - pos1.x();
			qreal vy = pos2.y() - pos1.y();
			qreal dist = sqrt(vx * vx + vy * vy);
			if(qAbs(dist) < 0.01) dist = 0.01;
			//qreal factor = (100 - dist)/(dist * 3);
			qreal link_weight;
			if(currentLink->weight()/10 > 300)
				link_weight = 300;
			else link_weight = currentLink->weight()/10;

			qreal factor = (link_weight - dist)/(dist * 3);
			currentLink->from()->incSpeed(-factor * vx, -factor * vy);
			currentLink->to()->incSpeed(factor * vx, factor * vy);
		}


		qreal dx = 0, dy = 0;
		foreach(n2, _nodes) {
			if(n1 == n2)
				continue;
			QPointF pos1 = n1->centerPos();
			QPointF pos2 = n2->centerPos();
			qreal vx = pos1.x() - pos2.x();
			qreal vy = pos1.y() - pos2.y();
			qreal dist = vx*vx + vy*vy;

			if(dist == 0) {
				dx += qrand() * 1.0 / RAND_MAX;
				dy += qrand() * 1.0 / RAND_MAX;
			} else if(dist < 100*100) {
				dx += vx / dist;
				dy += vy / dist;
			}
		}

		qreal dist = dx*dx + dy*dy;
		if(dist > 0) {
			dist = sqrt(dist) /2;
			n1->incSpeed(10 * dx / dist, 10 * dy / dist);
		}

		//Gravity
		//n1->incSpeed(0, 1);

		n1->updatePosition();
	}
}

}
