#ifndef ZCHUNKDEVICE_H
#define ZCHUNKDEVICE_H

#include <QIODevice>
#include <QByteArray>
#include <deque>
#include <future>

/*
    A ZLIB-COMPRESSED STREAM IN CHUNKS, as a QIODevice, so a QDataStream can be put on top
    of it unchanged (the library snapshot, Datamodel/librarysnapshot.cpp).

    ON DISK: [quint32 big-endian n][n bytes of qCompress output] ... [quint32 0]. Each
    chunk is qCompress(level) of up to chunkSize bytes, so memory stays at a few chunks
    however large the stream, where one qCompress over the whole of it would hold the
    whole of it twice. The 0 marks a stream that was closed cleanly; a stream without one
    is truncated, and reads past the last chunk fail rather than end quietly.

    OVERLAPPED. Writing, full chunks are compressed on worker threads, up to
    kMaxInFlight at once, while the caller fills the next, and written in order;
    reading, the next chunk is decompressed while the caller parses this one (inflate is
    fast enough that one ahead hides it). Measured on the 155,216-row Library snapshot:
    one chunk in flight made the write 0.79 s, against 0.41 s uncompressed.

    The inner device is not owned and must stay open for this device's lifetime; close()
    (or the destructor) writes the last chunk and the end mark. Sequential only.
*/
class ZChunkDevice : public QIODevice
{
public:
    explicit ZChunkDevice(QIODevice *inner, int chunkSize = 4 << 20, int level = 1);
    ~ZChunkDevice() override;

    bool open(OpenMode mode) override;
    void close() override;
    bool isSequential() const override { return true; }
    // a read-side error (corrupt or truncated stream), empty when there is none
    QString streamError() const { return mError; }

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    bool flushChunk();                  // write side: hand the buffer to a worker
    bool writeOldest();                 // write side: wait for and write the oldest result
    bool writePending();                // write side: all of them, in order
    bool fetchChunk();                  // read side: move the read-ahead into the buffer
    void startReadAhead();
    QByteArray readCompressed(bool &end);

    QIODevice *mInner;
    int mChunk;
    int mLevel;
    QByteArray mBuf;                    // write: being filled; read: being consumed
    qint64 mPos = 0;                    // read: consumed so far in mBuf
    std::future<QByteArray> mPending;   // read: the chunk being decompressed
    std::deque<std::future<QByteArray>> mInFlight;  // write: compressing, in file order
    static constexpr size_t kMaxInFlight = 4;
    bool mEnded = false;                // read: the end mark has been seen
    bool mFailed = false;
    QString mError;
};

#endif // ZCHUNKDEVICE_H
