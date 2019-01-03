﻿#ifndef MISCUTILS_H
#define MISCUTILS_H

#include <QString>
#include <QStringList>

extern QString PrintSystemInfo();
extern QString hexToString(char *buf, int size);
extern void getIp(QStringList &ipList);
extern void delayMs(unsigned int msec);
extern QString TrimIPv6Address(const QString& addr);

#endif // MISCUTILS_H
