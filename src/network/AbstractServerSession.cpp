#include "AbstractServerSession.h"

AbstractServerSession::AbstractServerSession(qint64 uid, QObject *parent)
    : QObject(parent), m_uid(uid) {}

qint64 AbstractServerSession::uid() const { return m_uid; }
