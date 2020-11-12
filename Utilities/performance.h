#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#include <QtWidgets>

class Performance
{
public:
    Performance();
    static double mediaReadSpeed(QFile &file);
};

#endif // PERFORMANCE_H
