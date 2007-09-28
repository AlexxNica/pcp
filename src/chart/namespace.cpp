/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtGui/QApplication>
#include <QtGui/QMessageBox>
#include <QtGui/QListView>
#include "namespace.h"
#include "kmchart.h"
#include "console.h"
#include "source.h"
#include "chart.h"

NameSpace::NameSpace(NameSpace *parent, QString name, bool inst, bool arch)
    : QTreeWidgetItem(parent, QTreeWidgetItem::UserType)
{
    my.expanded = false;
    my.back = parent;
    my.context = parent->my.context;
    my.basename = name;
    if (name == QString::null)
	my.type = ChildMinder;
    else if (!inst)
	my.type = NoType;
    else {
	my.type = InstanceName;
	QFont font = QTreeWidgetItem::font(0);
	font.setItalic(true);
	setFont(0, font);
    }
    my.isArchive = arch;

    console->post(KmChart::DebugGUI, "Added non-root namespace node %s",
		  (const char *)my.basename.toAscii());
    setText(0, my.basename);
}

NameSpace::NameSpace(QTreeWidget *list, const QmcContext *ctxt, bool arch)
    : QTreeWidgetItem(list, QTreeWidgetItem::UserType)
{
    my.expanded = false;
    my.back = this;
    my.context = (QmcContext *)ctxt;
    my.basename = Source::makeComboText(ctxt);
    if ((my.isArchive = arch) == true) {
	my.icon = QIcon(":/archive.png");
	my.type = ArchiveRoot;
    }
    else {
	my.icon = QIcon(":/computer.png");
	my.type = HostRoot;
    }
    console->post(KmChart::DebugGUI,
		  "Added root %s namespace node %s", my.isArchive ?
		  "archive" : "host", (const char *)my.basename.toAscii());
    setText(0, my.basename);
    setIcon(0, my.icon);
}

QString NameSpace::sourceName()
{
    return Source::makeSourceBaseName(my.context);
}

QString NameSpace::metricName()
{
    QString s;

    if (my.back->isRoot())
	s = text(0);
    else if (my.type == InstanceName)
	s = my.back->metricName();
    else {
	s = my.back->metricName();
	s.append(".");
	s.append(text(0));
    }
    return s;
}

QString NameSpace::instanceName()
{
    if (my.type == InstanceName)
	return text(0);
    return QString::null;
}

void NameSpace::setExpanded(bool expand)
{
    console->post(KmChart::DebugGUI,
		  "NameSpace::setExpanded on %p %s (expanded=%s, expand=%s)",
		  this, (const char *)metricName().toAscii(),
		  my.expanded ? "y" : "n", expand ? "y" : "n");

    if (expand && !my.expanded) {
	NameSpace *kid = (NameSpace *)child(0);

	if (kid && kid->isChildMinder()) {
	    takeChild(0);
	    delete kid;
	}
	my.expanded = true;
	pmUseContext(my.context->handle());
 
	if (my.type == LeafWithIndom)
	    expandInstanceNames();
	else if (my.type != InstanceName) {
	    expandMetricNames(isRoot() ? "" : metricName());
	    sortChildren(0, Qt::AscendingOrder);
	}
    }

    QTreeWidgetItem::setExpanded(expand);
}

void NameSpace::setSelectable(bool selectable)
{
    if (selectable)
	setFlags(flags() | Qt::ItemIsSelectable);
    else
	setFlags(flags() & ~Qt::ItemIsSelectable);
}

void NameSpace::setExpandable(bool expandable)
{
    console->post(KmChart::DebugGUI, "NameSpace::setExpandable "
		  "on %p %s (expanded=%s, expandable=%s)",
		  this, (const char *)metricName().toAscii(),
		  my.expanded ? "y" : "n", expandable ? "y" : "n");

    // NOTE: QT4.3 has setChildIndicatorPolicy for this workaround, but we want
    // to work on QT4.2 as well (this is used on Debian 4.0 - i.e. my laptop!).
    // This is the ChildMinder workaround - we insert a "dummy" child into items
    // that we know have children (since we have no way to explicitly set it and
    // we want to delay finding the actual children as late as possible).
    // When we later do fill in the real kids, we first delete the ChildMinder.

    if (expandable)
	addChild(new NameSpace(this, QString::null, false, my.isArchive));
}

static char *namedup(const char *name, const char *suffix)
{
    char *n;

    n = (char *)malloc(strlen(name) + 1 + strlen(suffix) + 1);
    sprintf(n, "%s.%s", name, suffix);
    return n;
}

void NameSpace::expandMetricNames(QString parent)
{
    char	**offspring = NULL;
    int		*status = NULL;
    pmID	*pmidlist = NULL;
    int		i, nleaf = 0;
    int		sts, noffspring;
    NameSpace	*m, **leaflist = NULL;
    char	*name = strdup(parent.toAscii());

    sts = pmGetChildrenStatus(name, &offspring, &status);
    if (sts < 0) {
	QString msg = QString();
	if (isRoot())
	    msg.sprintf("Cannot get metric names from source\n%s: %s.\n\n",
		(const char *)my.basename.toAscii(), pmErrStr(sts));
	else
	    msg.sprintf("Cannot get children of node\n\"%s\".\n%s.\n\n",
		name, pmErrStr(sts));
	QMessageBox::warning(NULL, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default | QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
	goto done;
    }
    else {
	noffspring = sts;
    }

    for (i = 0; i < noffspring; i++) {
	m = new NameSpace(this, offspring[i], false, my.isArchive);

	if (status[i] == PMNS_NONLEAF_STATUS) {
	    m->setSelectable(false);
	    m->setExpandable(true);
	    m->my.type = NonLeafName;
	}
	else {
	    // type still not set, could be LEAF_NULL_INDOM or LEAF_WITH_INDOM
	    // here we add this NameSpace pointer to the leaf list, and also
	    // modify the offspring list to only contain names (when done).
	    leaflist = (NameSpace **)realloc(leaflist,
					    (nleaf + 1) * sizeof(*leaflist));
	    leaflist[nleaf] = m;
	    offspring[nleaf] = namedup(name, offspring[i]);
	    nleaf++;
	}
    }

    if (nleaf == 0) {
	my.expanded = true;
	goto done;
    }

    pmidlist = (pmID *)malloc(nleaf * sizeof(*pmidlist));
    if ((sts = pmLookupName(nleaf, offspring, pmidlist)) < 0) {
	QString msg = QString();
	msg.sprintf("Cannot find PMIDs for \"%s\".\n%s.\n\n",
		name, pmErrStr(sts));
	QMessageBox::warning(NULL, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default | QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
	goto done;
    }
    else {
	for (i = 0; i < nleaf; i++) {
	    m = leaflist[i];
	    sts = pmLookupDesc(pmidlist[i], &m->my.desc);
	    if (sts < 0) {
		QString msg = QString();
		msg.sprintf("Cannot find metric descriptor at \"%s\".\n%s.\n\n",
			offspring[i], pmErrStr(sts));
		QMessageBox::warning(NULL, pmProgname, msg,
			QMessageBox::Ok | QMessageBox::Default |
				QMessageBox::Escape,
			QMessageBox::NoButton, QMessageBox::NoButton);
		goto done;
	    }
	    if (m->my.desc.indom == PM_INDOM_NULL) {
		m->my.type = LeafNullIndom;
		m->setExpandable(false);
		m->setSelectable(true);
	    }
	    else {
		m->my.type = LeafWithIndom;
		m->setExpandable(true);
		m->setSelectable(false);
	    }
	}
	my.expanded = true;
    }
    for (i = 0; i < nleaf; i++)
	free(offspring[i]);

done:
    if (pmidlist)
	free(pmidlist);
    if (leaflist)
	free(leaflist);
    if (offspring)
	free(offspring);
    if (status)
	free(status);
    free(name);
}

void NameSpace::expandInstanceNames()
{
    int		sts, i;
    int		ninst = 0;
    int		*instlist = NULL;
    char	**namelist = NULL;

    sts = !my.isArchive ? pmGetInDom(my.desc.indom, &instlist, &namelist) :
		pmGetInDomArchive(my.desc.indom, &instlist, &namelist);
    if (sts < 0) {
	QString msg = QString();
	msg.sprintf("Error fetching instance domain at node \"%s\".\n%s.\n\n",
		(const char *)metricName().toAscii(), pmErrStr(sts));
	QMessageBox::warning(NULL, pmProgname, msg,
		QMessageBox::Ok | QMessageBox::Default |
			QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    else {
	ninst = sts;
	my.expanded = true;
    }

    for (i = 0; i < ninst; i++) {
	NameSpace *m = new NameSpace(this, namelist[i], true, my.isArchive);

	m->setExpandable(false);
	m->setSelectable(true);
	m->my.instid = instlist[i];
    }

    if (instlist)
	free(instlist);
    if (namelist)
	free(namelist);
}

QString NameSpace::text(int column) const
{
    if (column > 0)
	return QString::null;
    return my.basename;
}

QIcon NameSpace::icon(int) const
{
    return my.icon;
}

void NameSpace::setIcon(int i, const QIcon &icon)
{
    my.icon = icon;
    QTreeWidgetItem::setIcon(i, icon);
}

NameSpace *NameSpace::dup(QTreeWidget *list)
{
    NameSpace *n;

    n = new NameSpace(list, my.context, my.type == ArchiveRoot);
    n->my.expanded = true;
    n->setSelectable(false);
    return n;
}

NameSpace *NameSpace::dup(QTreeWidget *, NameSpace *tree)
{
    NameSpace *n;

    n = new NameSpace(tree, my.basename, my.type == InstanceName, my.isArchive);
    n->my.expanded = true;
    n->my.context = my.context;
    n->my.instid = my.instid;
    n->my.desc = my.desc;
    n->my.type = my.type;

    if (my.type == NoType || my.type == ChildMinder) {
	console->post("NameSpace::dup bad type=%d on %p %s)",
		  my.type, this, (const char *)metricName().toAscii());
	abort();
    }
    else if (!isLeaf()) {
	n->setSelectable(false);
    }
    else {
	n->setSelectable(true);

	QColor c = Chart::defaultColor(-1);
	n->setOriginalColor(c);
	n->setCurrentColor(c, NULL);

	// this is a leaf so walk back up to the root, opening each node up
	NameSpace *up;
	for (up = tree; up->my.back != up; up = up->my.back) {
	    up->my.expanded = true;
	    up->setExpanded(true);
	}
	up->my.expanded = true;
	up->setExpanded(true);	// add the host/archive root as well.

	n->setSelected(true);
    }
    return n;
}

bool NameSpace::cmp(QTreeWidgetItem *item)
{
    if (!item)	// empty list
	return false;
    return (item->text(0) == my.basename);
}

void NameSpace::addToTree(QTreeWidget *target)
{
    QList<NameSpace *> nodelist;
    NameSpace *node;

    // Walk through each component of this name, creating them in the
    // target list (if not there already), right down to the leaf.
    // We hold everything in a temporary list since we need to add to
    // the target in root-to-leaf order but we're currently at a leaf.

    for (node = this; node->my.back != node; node = node->my.back)
	nodelist.prepend(node);
    nodelist.prepend(node);	// add the host/archive root as well.

    NameSpace *tree = (NameSpace *)target->invisibleRootItem();
    QTreeWidgetItem *item = NULL;

    for (int i, n = 0; n < nodelist.size(); n++) {
	node = nodelist.at(n);
	for (i = 0; i < tree->childCount(); i++) {
	    item = (NameSpace *)tree->child(i);
	    if (node->cmp(item)) {
		// no insert at this level necessary, move down a level
		if (!node->isLeaf()) {
		    tree = (NameSpace *)item;
		    item = item->child(0);
		}
		else {	// already there, just select the existing name
		    item->setSelected(true);
		}
		break;
	    }
	}

	// When no more children and no match so far, we dup & insert
	if (i == tree->childCount()) {
	    if (node->isRoot()) {
		tree = node->dup(target);
	    }
	    else if (tree) {
		tree = node->dup(target, tree);
	    }
	}
    }
}

void NameSpace::removeFromTree(QTreeWidget *)
{
    NameSpace *self, *tree, *node = this;

    this->setSelected(false);
    do {
	self = node;
	tree = node->my.back;
	if (node->childCount() == 0)
	    delete node;
	node = tree;
    } while (self != tree);
}

void NameSpace::setCurrentColor(QColor color, QTreeWidget *treeview)
{
    QPixmap pix(8, 8);

    // Set the current metric color, and (optionally) move on to the next
    // leaf node in the view (if given treeview parameter is non-NULL).
    // We're taking a punt here that the user will move down through the
    // metrics just added, and set their prefered color on each one; so,
    // by doing the next selection automatically, we save some clicks.

    my.current = color;
    pix.fill(color);
    setIcon(0, QIcon(pix));

    if (treeview) {
	QTreeWidgetItemIterator it(this, QTreeWidgetItemIterator::Selectable);
	if (*it) {
	    (*it)->setSelected(true);
	    this->setSelected(false);
	}
    }
}
