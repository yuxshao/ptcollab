#include "ServerSession.h"

ServerSession::ServerSession(qint64 uid, QObject *parent)
    : QObject(parent), m_uid(uid) {}

qint64 ServerSession::uid() const { return m_uid; }
