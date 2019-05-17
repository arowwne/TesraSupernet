



#include "peertablemodel.h"

#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"

#include "net.h"
#include "sync.h"

#include <QDebug>
#include <QList>
#include <QTimer>

bool NodeLessThan::operator()(const CNodeCombinedStats& left, const CNodeCombinedStats& right) const
{
    const CNodeStats* pLeft = &(left.nodeStats);
    const CNodeStats* pRight = &(right.nodeStats);

    if (order == Qt::DescendingOrder)
        std::swap(pLeft, pRight);

    switch (column) {
    case PeerTableModel::Address:
        return pLeft->addrName.compare(pRight->addrName) < 0;
    case PeerTableModel::Subversion:
        return pLeft->cleanSubVer.compare(pRight->cleanSubVer) < 0;
    case PeerTableModel::Ping:
        return pLeft->dPingTime < pRight->dPingTime;
    }

    return false;
}


class PeerTablePriv
{
public:
    
    QList<CNodeCombinedStats> cachedNodeStats;
    
    int sortColumn;
    
    Qt::SortOrder sortOrder;
    
    std::map<NodeId, int> mapNodeRows;

    
    void refreshPeers()
    {
        {
            TRY_LOCK(cs_vNodes, lockNodes);
            if (!lockNodes) {
                
                return;
            }
            cachedNodeStats.clear();
#if QT_VERSION >= 0x040700
            cachedNodeStats.reserve(vNodes.size());
#endif
            BOOST_FOREACH (CNode* pnode, vNodes) {
                CNodeCombinedStats stats;
                stats.nodeStateStats.nMisbehavior = 0;
                stats.nodeStateStats.nSyncHeight = -1;
                stats.fNodeStateStatsAvailable = false;
                pnode->copyStats(stats.nodeStats);
                cachedNodeStats.append(stats);
            }
        }

        
        {
            TRY_LOCK(cs_main, lockMain);
            if (lockMain) {
                BOOST_FOREACH (CNodeCombinedStats& stats, cachedNodeStats)
                    stats.fNodeStateStatsAvailable = GetNodeStateStats(stats.nodeStats.nodeid, stats.nodeStateStats);
            }
        }

        if (sortColumn >= 0)
            
            qStableSort(cachedNodeStats.begin(), cachedNodeStats.end(), NodeLessThan(sortColumn, sortOrder));

        
        mapNodeRows.clear();
        int row = 0;
        BOOST_FOREACH (CNodeCombinedStats& stats, cachedNodeStats)
            mapNodeRows.insert(std::pair<NodeId, int>(stats.nodeStats.nodeid, row++));
    }

    int size()
    {
        return cachedNodeStats.size();
    }

    CNodeCombinedStats* index(int idx)
    {
        if (idx >= 0 && idx < cachedNodeStats.size()) {
            return &cachedNodeStats[idx];
        } else {
            return 0;
        }
    }
};

PeerTableModel::PeerTableModel(ClientModel* parent) : QAbstractTableModel(parent),
                                                      clientModel(parent),
                                                      timer(0)
{
    columns << tr("Address/Hostname") << tr("Version") << tr("Ping Time");
    priv = new PeerTablePriv();
    
    priv->sortColumn = -1;

    
    timer = new QTimer();
    connect(timer, SIGNAL(timeout()), SLOT(refresh()));
    timer->setInterval(MODEL_UPDATE_DELAY);

    
    refresh();
}

void PeerTableModel::startAutoRefresh()
{
    timer->start();
}

void PeerTableModel::stopAutoRefresh()
{
    timer->stop();
}

int PeerTableModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int PeerTableModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return columns.length();
    ;
}

QVariant PeerTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    CNodeCombinedStats* rec = static_cast<CNodeCombinedStats*>(index.internalPointer());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case Address:
            return QString::fromStdString(rec->nodeStats.addrName);
        case Subversion:
            return QString::fromStdString(rec->nodeStats.cleanSubVer);
        case Ping:
            return GUIUtil::formatPingTime(rec->nodeStats.dPingTime);
        }
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == Ping)
            return (int)(Qt::AlignRight | Qt::AlignVCenter);
    }

    return QVariant();
}

QVariant PeerTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole && section < columns.size()) {
            return columns[section];
        }
    }
    return QVariant();
}

Qt::ItemFlags PeerTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return 0;

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    return retval;
}

QModelIndex PeerTableModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    CNodeCombinedStats* data = priv->index(row);

    if (data) {
        return createIndex(row, column, data);
    } else {
        return QModelIndex();
    }
}

const CNodeCombinedStats* PeerTableModel::getNodeStats(int idx)
{
    return priv->index(idx);
}

void PeerTableModel::refresh()
{
    emit layoutAboutToBeChanged();
    priv->refreshPeers();
    emit layoutChanged();
}

int PeerTableModel::getRowByNodeId(NodeId nodeid)
{
    std::map<NodeId, int>::iterator it = priv->mapNodeRows.find(nodeid);
    if (it == priv->mapNodeRows.end())
        return -1;

    return it->second;
}

void PeerTableModel::sort(int column, Qt::SortOrder order)
{
    priv->sortColumn = column;
    priv->sortOrder = order;
    refresh();
}
