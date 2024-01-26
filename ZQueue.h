#pragma once

#include <QList>
#include <QMutex>
#include <QMutexLocker>

template <class T> class ZQueue : public QList<T>
{
public:
	// compiler-generated special member functions are fine!
	inline void swap(QQueue<T>& other) noexcept { QMutexLocker locker(&m_semaphore); QList<T>::swap(other); } // prevent QList<->QQueue swaps
	inline void enqueue(const T& t) { QMutexLocker locker(&m_semaphore); QList<T>::append(t); }
	inline T dequeue() { QMutexLocker locker(&m_semaphore); return QList<T>::takeFirst(); }
	inline T& head() { QMutexLocker locker(&m_semaphore); return QList<T>::first(); }
	inline const T& head() const { QMutexLocker locker(&m_semaphore); return QList<T>::first(); }
private:
	QMutex m_semaphore;
};

