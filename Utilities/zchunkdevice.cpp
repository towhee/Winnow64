#include "Utilities/zchunkdevice.h"

#include <QtEndian>

namespace {
// a corrupt length must not make us allocate the machine: no real chunk comes near it
constexpr quint32 kMaxCompressedChunk = 256u << 20;
}

ZChunkDevice::ZChunkDevice(QIODevice *inner, int chunkSize, int level)
    : mInner(inner), mChunk(qMax(4096, chunkSize)), mLevel(level)
{
}

ZChunkDevice::~ZChunkDevice()
{
    if (isOpen()) close();
}

bool ZChunkDevice::open(OpenMode mode)
{
    if (!mInner) return false;
    if ((mode & ReadWrite) == ReadWrite) return false;      // one direction only
    mBuf.clear();
    mPos = 0;
    mEnded = mFailed = false;
    mError.clear();
    if (!QIODevice::open(mode | Unbuffered)) return false;
    if (mode & ReadOnly) startReadAhead();
    else mBuf.reserve(mChunk);
    return true;
}

void ZChunkDevice::close()
{
    if (!isOpen()) return;
    if (openMode() & WriteOnly) {
        if (!mBuf.isEmpty()) flushChunk();
        writePending();
        const quint32 end = 0;
        uchar be[4];
        qToBigEndian(end, be);
        if (mInner->write(reinterpret_cast<const char *>(be), 4) != 4) mFailed = true;
    }
    else if (mPending.valid()) {
        mPending.wait();                // never leave a worker reading a closing device
    }
    QIODevice::close();
}

qint64 ZChunkDevice::writeData(const char *data, qint64 maxSize)
{
    if (mFailed) return -1;
    qint64 done = 0;
    while (done < maxSize) {
        const qint64 room = mChunk - mBuf.size();
        const qint64 n = qMin(room, maxSize - done);
        mBuf.append(data + done, n);
        done += n;
        if (mBuf.size() >= mChunk && !flushChunk()) return -1;
    }
    return done;
}

bool ZChunkDevice::flushChunk()
{
    // bounded: memory stays at kMaxInFlight chunks plus the one being filled
    if (mInFlight.size() >= kMaxInFlight && !writeOldest()) return false;
    QByteArray chunk;
    chunk.swap(mBuf);
    mBuf.reserve(mChunk);
    const int level = mLevel;
    mInFlight.push_back(std::async(std::launch::async, [chunk = std::move(chunk), level] {
        return qCompress(chunk, level);
    }));
    return !mFailed;
}

bool ZChunkDevice::writeOldest()
{
    if (mInFlight.empty()) return !mFailed;
    const QByteArray z = mInFlight.front().get();
    mInFlight.pop_front();
    if (mFailed) return false;
    uchar be[4];
    qToBigEndian(quint32(z.size()), be);
    if (mInner->write(reinterpret_cast<const char *>(be), 4) != 4
        || mInner->write(z) != z.size())
        mFailed = true;
    return !mFailed;
}

bool ZChunkDevice::writePending()
{
    bool ok = !mFailed;
    while (!mInFlight.empty()) ok = writeOldest() && ok;
    return ok;
}

QByteArray ZChunkDevice::readCompressed(bool &end)
{
    end = false;
    uchar be[4];
    if (mInner->read(reinterpret_cast<char *>(be), 4) != 4) {
        mError = "truncated (no end mark)";
        return QByteArray();
    }
    const quint32 n = qFromBigEndian<quint32>(be);
    if (n == 0) { end = true; return QByteArray(); }
    if (n > kMaxCompressedChunk) { mError = "corrupt chunk length"; return QByteArray(); }
    QByteArray z = mInner->read(n);
    if (quint32(z.size()) != n) { mError = "truncated chunk"; return QByteArray(); }
    return z;
}

void ZChunkDevice::startReadAhead()
{
    /*  The inner READ stays on this thread (the inner device is not thread-safe); only
        the decompression goes to the worker. */
    bool end = false;
    QByteArray z = readCompressed(end);
    if (end) { mEnded = true; return; }
    if (z.isEmpty()) { mFailed = true; return; }
    mPending = std::async(std::launch::async, [z = std::move(z)] { return qUncompress(z); });
}

bool ZChunkDevice::fetchChunk()
{
    if (mFailed || mEnded || !mPending.valid()) return false;
    mBuf = mPending.get();
    mPos = 0;
    if (mBuf.isEmpty()) {
        mFailed = true;
        mError = "corrupt chunk";
        return false;
    }
    startReadAhead();
    return true;
}

qint64 ZChunkDevice::readData(char *data, qint64 maxSize)
{
    qint64 done = 0;
    while (done < maxSize) {
        if (mPos >= mBuf.size() && !fetchChunk()) break;
        const qint64 n = qMin<qint64>(mBuf.size() - mPos, maxSize - done);
        memcpy(data + done, mBuf.constData() + mPos, size_t(n));
        mPos += n;
        done += n;
    }
    if (done == 0 && mFailed) return -1;
    return done;
}
