// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QStandardItemModel>
#include <QTreeView>
#include <QTimer>
#include <QScrollArea>
#include <QScrollBar>
#include <cstdio>
#include "Widgets/Extended/RDHeaderView.h"
#include "Code/QRDUtils.h"
// This isolated widget probe uses Qt's lifetime-aware queued callback directly.
void GUIInvoke::defer(QObject *object, const std::function<void()> &fn)
{ QTimer::singleShot(0, object, fn); }
class View : public QTreeView
{
public:
  mutable int measurements=0;
  int sizeHintForColumn(int column) const override
  { measurements++; return QTreeView::sizeHintForColumn(column); }
};
int main(int argc,char **argv)
{
 QApplication app(argc,argv);
 QStandardItemModel oldModel(2,2),newModel(2,2);
 oldModel.setData(oldModel.index(0,0),"old");newModel.setData(newModel.index(0,0),"new");
 View view; auto header=new RDHeaderView(Qt::Horizontal,&view); view.setHeader(header);
 QScrollArea scroll; scroll.horizontalScrollBar()->setRange(0,10);
 header->setPinnedColumns(0,&scroll);
 view.setModel(&oldModel); header->setColumnStretchHints({1,1});
 view.setModel(&newModel); view.measurements=0;
 oldModel.setData(oldModel.index(0,0),"retired model change");
 if(view.measurements) {fprintf(stderr,"FAIL retired model resized current header: %d\n",view.measurements); return 2;}
 newModel.setData(newModel.index(0,0),"current model change");
 if(!view.measurements) {fprintf(stderr,"FAIL current model did not resize header\n");return 3;}
 // Replacing the header leaves the view and models alive; old functor slots must disconnect.
 view.setHeader(new QHeaderView(Qt::Horizontal,&view));
 scroll.horizontalScrollBar()->setValue(5);
 newModel.setData(newModel.index(0,0),"change after old header destruction");
 view.expand(newModel.index(0,0)); view.collapse(newModel.index(0,0));
 app.processEvents(); puts("PASS retired/current models and header destruction");
}
