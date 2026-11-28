#ifndef BACKGROUNDPRIORITY_H
#define BACKGROUNDPRIORITY_H

#include <QThread>

#if defined(Q_OS_MAC)
#include <pthread.h>
#include <pthread/qos.h>
#endif

/*
    BACKGROUND PRIORITY FOR SPECULATIVE WORK. An RAII guard that lowers the calling
    thread's scheduling priority for its lifetime and restores it on exit. Used by an
    autoDevPreviews step (MW::developPixelSource with a cancel token): the step runs on
    developRenderPool, whose single thread also runs the user's own Develop renders, so
    the priority is lowered for the job and put back, never left behind.

    PER PLATFORM:

      macOS    QoS class UTILITY (pthread_set_qos_class_self_np). This is the one that
               reaches the render's inner loops too: OpenCV's cv::parallel_for_ runs on
               GCD there, and dispatch_apply inherits the QoS of the thread that calls
               it, so the whole fan-out drops below the decoders feeding the loupe. The
               previous class (qos_class_self) is restored.
      Windows  QThread::LowPriority (THREAD_PRIORITY_BELOW_NORMAL) on this thread.
               OpenCV's own worker pool does not inherit it.
      Linux    QThread::LowPriority on this thread. Under the default scheduler
               (SCHED_OTHER) Qt documents the priority as ignored, so expect little or
               no effect; it is set for symmetry and for systems that honour it.
               OpenCV's pool does not inherit it.

    On Windows and Linux the guard therefore covers only the thread's own work; the
    gating in Main/autodevpreviews.cpp (wait for a pause, wait for the read-ahead,
    give way to input, abandon on lag) is what keeps a step out of the user's way
    there. Measure before relying on it: [AutoDevPreview] lines under the perf probe.
*/
class BackgroundPriority
{
public:
    explicit BackgroundPriority(bool on) : active(on)
    {
        if (!active) return;
#if defined(Q_OS_MAC)
        prevQos = qos_class_self();
        pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#elif defined(Q_OS_WIN) || defined(Q_OS_LINUX)
        if (QThread *t = QThread::currentThread()) {
            prevPriority = t->priority();
            /* A pool thread never given one reports InheritPriority, which setPriority
               refuses (with a warning) -- restore Normal instead. */
            if (prevPriority == QThread::InheritPriority)
                prevPriority = QThread::NormalPriority;
            t->setPriority(QThread::LowPriority);
        }
#endif
    }
    ~BackgroundPriority()
    {
        if (!active) return;
#if defined(Q_OS_MAC)
        pthread_set_qos_class_self_np(prevQos, 0);
#elif defined(Q_OS_WIN) || defined(Q_OS_LINUX)
        if (QThread *t = QThread::currentThread()) t->setPriority(prevPriority);
#endif
    }
    BackgroundPriority(const BackgroundPriority &) = delete;
    BackgroundPriority &operator=(const BackgroundPriority &) = delete;

private:
    bool active = false;
#if defined(Q_OS_MAC)
    qos_class_t prevQos = QOS_CLASS_UNSPECIFIED;
#elif defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    QThread::Priority prevPriority = QThread::InheritPriority;
#endif
};

#endif // BACKGROUNDPRIORITY_H
