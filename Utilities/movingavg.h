#ifndef MOVINGAVG_H
#define MOVINGAVG_H

#pragma once
#include <QtCore/QVector>
#include <QtGlobal>

namespace Winnow::Util {

class MovingAvg {
public:
    explicit MovingAvg(int window = 10) noexcept
        : _cap(qMax(1, window)), _buf(_cap, 0), _head(0), _size(0), _sum(0) {}

    inline void setWindow(int window) noexcept {
        window = qMax(1, window);
        if (window == _cap) return;

        QVector<int> newest;
        newest.reserve(qMin(_size, window));
        for (int i = 0; i < qMin(_size, window); ++i) {
            int idx = (_head - 1 - i + _cap) % _cap;
            newest.push_back(_buf[idx]);
        }

        _cap  = window;
        _buf  = QVector<int>(_cap, 0);
        _head = 0;
        _size = 0;
        _sum  = 0;

        for (int i = newest.size() - 1; i >= 0; --i) push(newest[i]);
    }

    inline void push(int v) noexcept {
        if (_size < _cap) {
            _buf[_head] = v;
            _sum += v;
            _head = (_head + 1 == _cap) ? 0 : _head + 1;
            ++_size;
        } else {
            int old = _buf[_head];
            _sum += (v - old);
            _buf[_head] = v;
            _head = (_head + 1 == _cap) ? 0 : _head + 1;
        }
    }

    inline int avg() const noexcept {
        return _size ? int((_sum + _size / 2) / _size) : 0; // rounded
    }

    inline int window() const noexcept { return _cap; }
    inline int size()   const noexcept { return _size; }

private:
    int          _cap;     // capacity (window)
    QVector<int> _buf;     // ring buffer
    int          _head;    // next write (oldest element)
    int          _size;    // valid samples
    qint64       _sum;     // running sum
};

} // namespace Winnow::Utilities
#endif // MOVINGAVG_H
